#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFFormValue.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/RelocVisitor.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstring>
#include <list>
#include <string>
#include <system_error>

using namespace llvm;
using namespace object;

static std::string tripleName;

static const Target *getTarget(const ObjectFile *Obj = nullptr) {
  // Figure out the target triple.
  llvm::Triple TheTriple("unknown-unknown-unknown");
  if (tripleName.empty()) {
    if (Obj) {
      TheTriple.setArch(Triple::ArchType(Obj->getArch()));
      // TheTriple defaults to ELF, and COFF doesn't have an environment:
      // the best we can do here is indicate that it is mach-o.
      if (Obj->isMachO())
        TheTriple.setObjectFormat(Triple::MachO);

      if (Obj->isCOFF()) {
        const auto COFFObj = dyn_cast<COFFObjectFile>(Obj);
        if (COFFObj->getArch() == Triple::thumb)
          TheTriple.setTriple("thumbv7-windows");
      }
    }
  } else {
    //std::string str = Triple::normalize(tripleName);
    std::string str = TheTriple.normalize(tripleName);
    TheTriple.setTriple(str);
    //TheTriple.setTriple("x86_64-unknown-linux-gnu");
  }

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget("", TheTriple,
                                                         Error);
  if (!TheTarget) {
    errs() << Error;
    return nullptr;
  }

  // Update the triple name and return the found target.
  tripleName = TheTriple.getTriple();
  return TheTarget;
}

std::string makeLocationString(const DWARFDebugInfoEntryMinimal *die,
                               const DWARFUnit *unit) {
  using namespace llvm::dwarf;
  unsigned fileno = die->getAttributeValueAsUnsignedConstant(unit,
    DW_AT_decl_file, 0);
  unsigned line = die->getAttributeValueAsUnsignedConstant(unit,
    DW_AT_decl_line, 0);
  unsigned column = die->getAttributeValueAsUnsignedConstant(unit,
    DW_AT_decl_column, 0);

  DWARFUnit *U = const_cast<DWARFUnit *>(unit);

  // The file number is an index into the file table. Find the actual filename
  // from the line table.
  std::string str;
  if (const auto *LT = unit->getContext().getLineTableForUnit(U))
    if (LT->getFileNameByIndex(fileno, U->getCompilationDir(),
          DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath, str)) {
      str += ':' + std::to_string(line);
      if (column > 0)
        str += ':' + std::to_string(column);
    }
  return str;
}

class TypeInfo {
public:
  struct FieldInfo {
    FieldInfo(std::shared_ptr<TypeInfo> type, uint64_t offset)
      : offset(offset), type(std::move(type)) {}
    uint64_t offset;
    std::shared_ptr<TypeInfo> type;
  };

  TypeInfo(StringRef name, uint64_t size) : name(name), size(size) {}

  StringRef getName() const { return name; }
  uint64_t getSize() const { return size; }

  std::vector<FieldInfo> &getFields() { return fields; }

private:
  std::string name;
  uint64_t size;
  std::vector<FieldInfo> fields;
};

class FunctionInfo final {
public:
  FunctionInfo(const DWARFDebugInfoEntryMinimal *die, const DWARFUnit *unit);

  uint64_t getAddress() const { return address; }
  std::string parseLocation(ArrayRef<uint8_t> value, const DWARFUnit *unit);

private:
  uint64_t address;
  unsigned framePointer;
};

FunctionInfo::FunctionInfo(const DWARFDebugInfoEntryMinimal *die,
                           const DWARFUnit *unit) {
  if (!die) {
    address = 0;
    framePointer = 0;
  } else {
    address = die->getAttributeValueAsAddress(unit, dwarf::DW_AT_low_pc, 0);
    // XXX: Parse DW_AT_frame_base for this.
    framePointer = 7;
  }
}

std::string FunctionInfo::parseLocation(ArrayRef<uint8_t> expression,
                                        const DWARFUnit *unit) {
  using namespace dwarf;

  // We need the MRI to map from DWARF register numbers to ASM names.
  std::unique_ptr<MCRegisterInfo> mri(
    getTarget()->createMCRegInfo(tripleName));

  uint32_t addrSize = unit->getAddressByteSize();
  DataExtractor extractor(
    StringRef((const char *)expression.data(), expression.size()),
    true, addrSize);
  uint32_t pc = 0;
  std::string program;
  raw_string_ostream prog_out(program);

  while (pc < expression.size()) {
    LocationAtom opcode = (LocationAtom)extractor.getU8(&pc);
    switch (opcode) {
    case DW_OP_fbreg: {
      int64_t offset = extractor.getSLEB128(&pc);
      prog_out << offset << "(%" <<
        mri->getName(mri->getLLVMRegNum(framePointer, false)) << ")";
      break;
    }
    case DW_OP_breg0:  case DW_OP_breg1:  case DW_OP_breg2:
    case DW_OP_breg3:  case DW_OP_breg4:  case DW_OP_breg5:
    case DW_OP_breg6:  case DW_OP_breg7:  case DW_OP_breg8:
    case DW_OP_breg9:  case DW_OP_breg10: case DW_OP_breg11:
    case DW_OP_breg12: case DW_OP_breg13: case DW_OP_breg14:
    case DW_OP_breg15: case DW_OP_breg16: case DW_OP_breg17:
    case DW_OP_breg18: case DW_OP_breg19: case DW_OP_breg20:
    case DW_OP_breg21: case DW_OP_breg22: case DW_OP_breg23:
    case DW_OP_breg24: case DW_OP_breg25: case DW_OP_breg26:
    case DW_OP_breg27: case DW_OP_breg28: case DW_OP_breg29:
    case DW_OP_breg30: case DW_OP_breg31: {
      int64_t offset = extractor.getSLEB128(&pc);
      prog_out << offset << "(%" <<
        mri->getName(mri->getLLVMRegNum(opcode - DW_OP_breg0, false)) <<
        ")";
      break;
    }
    case DW_OP_reg0:  case DW_OP_reg1:  case DW_OP_reg2:
    case DW_OP_reg3:  case DW_OP_reg4:  case DW_OP_reg5:
    case DW_OP_reg6:  case DW_OP_reg7:  case DW_OP_reg8:
    case DW_OP_reg9:  case DW_OP_reg10: case DW_OP_reg11:
    case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14:
    case DW_OP_reg15: case DW_OP_reg16: case DW_OP_reg17:
    case DW_OP_reg18: case DW_OP_reg19: case DW_OP_reg20:
    case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23:
    case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26:
    case DW_OP_reg27: case DW_OP_reg28: case DW_OP_reg29:
    case DW_OP_reg30: case DW_OP_reg31: {
      prog_out << "%" <<
        mri->getName(mri->getLLVMRegNum(opcode - DW_OP_reg0, false));
      break;
    }
    case DW_OP_addr: {
      uint64_t addr = extractor.getAddress(&pc);
      prog_out << "(" << format_hex(addr, addrSize + 2) << ")";
      break;
    }
    case DW_OP_deref: {
      prog_out << " [deref]";
      break;
    }
    case DW_OP_plus_uconst: {
      prog_out << " + " << extractor.getULEB128(&pc);
      break;
    }
    case DW_OP_piece: {
      uint64_t piecesSize = extractor.getULEB128(&pc);
      prog_out << " [" << piecesSize << "-byte chunk]";
      break;
    }
    case DW_OP_stack_value: {
      prog_out << " [known constant value]";
      break;
    }
    case DW_OP_const1u: case DW_OP_const2u: case DW_OP_const4u:
    case DW_OP_const8u: {
      int nBytes = 1 << ((opcode - DW_OP_const1u) / 2);
      uint64_t value = extractor.getUnsigned(&pc, nBytes);
      prog_out << value;
      break;
    }
    case DW_OP_constu: {
      prog_out << extractor.getULEB128(&pc);
      break;
    }
    case DW_OP_const1s: case DW_OP_const2s: case DW_OP_const4s:
    case DW_OP_const8s: {
      int nBytes = 1 << ((opcode - DW_OP_const1u) / 2);
      int64_t value = extractor.getSigned(&pc, nBytes);
      prog_out << value;
      break;
    }
    case DW_OP_consts: {
      prog_out << extractor.getSLEB128(&pc);
      break;
    }
    case DW_OP_lit0:  case DW_OP_lit1:  case DW_OP_lit2:  case DW_OP_lit3:
    case DW_OP_lit4:  case DW_OP_lit5:  case DW_OP_lit6:  case DW_OP_lit7:
    case DW_OP_lit8:  case DW_OP_lit9:  case DW_OP_lit10: case DW_OP_lit11:
    case DW_OP_lit12: case DW_OP_lit13: case DW_OP_lit14: case DW_OP_lit15:
    case DW_OP_lit16: case DW_OP_lit17: case DW_OP_lit18: case DW_OP_lit19:
    case DW_OP_lit20: case DW_OP_lit21: case DW_OP_lit22: case DW_OP_lit23:
    case DW_OP_lit24: case DW_OP_lit25: case DW_OP_lit26: case DW_OP_lit27:
    case DW_OP_lit28: case DW_OP_lit29: case DW_OP_lit30: case DW_OP_lit31: {
      prog_out << (opcode - DW_OP_lit0);
      break;
    }
    case DW_OP_GNU_push_tls_address: {
      prog_out << " [tls index]";
      break;
    }
    case DW_OP_and: {
      prog_out << " &";
      break;
    }
    case DW_OP_minus: {
      prog_out << " -";
      break;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    case 0xf3: { // DW_OP_GNU_entry_value, gcc's DWARF extension
      return "[value known only at entry]";
    }
#pragma GCC diagnostic pop
    default:
      const char *name = OperationEncodingString(opcode);
      if (name) {
        errs() << "UNKNOWN STRING " << OperationEncodingString(opcode) << '\n';
      } else {
        errs() << "UNKNOWN OPCODE " << format_hex(opcode, 2) << '\n';
      }
      errs() << "Full expression:";
      for (uint8_t byte : expression) {
        errs() << ' ' << format_hex_no_prefix(byte, 2);
      }
      errs() << '\n';
      if (expression.back() == DW_OP_stack_value) {
        return "[synthetic value, probably]";
      }
      abort();
    }
  }
  return prog_out.str();
}

class DwarfVariableFinder final {
public:
  DwarfVariableFinder(const DWARFUnit &u) : unit(&u) {}

  void findVariables(const DWARFDebugInfoEntryMinimal *die,
    const DWARFDebugInfoEntryMinimal *functionDie = nullptr);
  void printJSON(raw_ostream &out);
  void printTypeJSON(raw_ostream &out);

private:
  DenseMap<uint32_t, std::shared_ptr<TypeInfo>> types;
  std::vector<std::string> globals;
  std::vector<std::string> locals;

  void handleVarDie(const DWARFDebugInfoEntryMinimal *die,
    const DWARFDebugInfoEntryMinimal *functionDie);
  std::shared_ptr<TypeInfo> getType(const DWARFDebugInfoEntryMinimal *die,
      dwarf::Attribute tag) {
    uint64_t reference = die->getAttributeValueAsReference(unit, tag, 0);
    if (reference == 0)
      return std::make_shared<TypeInfo>("", ~0u);
    auto it = types.find(reference);
    if (it != types.end())
      return it->second;
    auto result = makeType(unit->getDIEForOffset(reference));
    types[reference] = result;
    return result;
  }
  std::shared_ptr<TypeInfo> makeType(const DWARFDebugInfoEntryMinimal *die);

  std::string parseLocationList(FunctionInfo &context, uint64_t secOffset);
  const DWARFUnit *unit;
};

void DwarfVariableFinder::findVariables(const DWARFDebugInfoEntryMinimal *die,
    const DWARFDebugInfoEntryMinimal *functionDie) {
  for (auto child = die->getFirstChild(); child; child = child->getSibling()) {
    const DWARFDebugInfoEntryMinimal *context = functionDie;
    switch (child->getTag()) {
    case dwarf::DW_TAG_variable:
    case dwarf::DW_TAG_formal_parameter:
    case dwarf::DW_TAG_constant:
      handleVarDie(child, context);
      break;
    case dwarf::DW_TAG_subprogram:
      context = child;
      // Yes, we want to fall though here!
    default:
      if (child->hasChildren())
        findVariables(child, context);
    }
  }
}

void DwarfVariableFinder::handleVarDie(const DWARFDebugInfoEntryMinimal *die,
    const DWARFDebugInfoEntryMinimal *functionDie) {
  const char *name = die->getName(unit, DINameKind::ShortName);
  if (!name) name = "";

  // XXX: lots of nasty repeated work here.
  FunctionInfo context(functionDie, unit);

  DWARFFormValue unparsedLocation;
  if (!die->getAttributeValue(unit, dwarf::DW_AT_location, unparsedLocation)) {
    // If we can't find a location value at all for the variable, then there's
    // no point in trying to do anything here.
    return;
  }

  std::string varLoc;
  if (unparsedLocation.isFormClass(DWARFFormValue::FC_SectionOffset)) {
    // This is a loclist pointer, i.e., its location changes depending on the
    // instruction pointer.
    uint64_t secOffset = *unparsedLocation.getAsSectionOffset();
    varLoc = parseLocationList(context, secOffset);
  } else {
    varLoc = '"' + context.parseLocation(*unparsedLocation.getAsBlock(), unit)
      + '"';
  }

  // Find the type of this variable.
  std::shared_ptr<TypeInfo> type = getType(die, dwarf::DW_AT_type);

  // Find the source location of the string.
  std::string sourceLoc = makeLocationString(die, unit);

  std::string jsonEntry;
  raw_string_ostream entry(jsonEntry);
  entry << "{\"type\": \"" << type->getName() << "\"";
  entry << ", \"name\": \"" << name << "\"";
  entry << ", \"location\": " << varLoc;
  if (context.getAddress())
    entry << ", \"function\": " << context.getAddress();
  if (!sourceLoc.empty())
    entry << ", \"source\": \"" << sourceLoc << "\"";
  entry << "}";

  (context.getAddress() ? locals : globals).push_back(entry.str());
}

std::shared_ptr<TypeInfo> DwarfVariableFinder::makeType(
    const DWARFDebugInfoEntryMinimal *die) {
  using namespace llvm::dwarf;

  // Compute the size of the data type. This attribute isn't always present,
  // though, but where it is, it's common to everybody.
  uint64_t size = die->getAttributeValueAsUnsignedConstant(unit,
    DW_AT_byte_size, -1);

  switch (die->getTag()) {
    // Pretend C++ references are the same as pointers.
    case DW_TAG_reference_type:
    case DW_TAG_rvalue_reference_type:
    case DW_TAG_pointer_type: {
      std::string subtype = getType(die, DW_AT_type)->getName();
      // The DIEs here seem to be empty... so void*?
      if (subtype == "<unknown>") {
        subtype = "void";
      }
      subtype += "*";
      // XXX: pointer size
      return std::make_shared<TypeInfo>(subtype, 8);
    }

    // CV-qualified types: ignore the parameters for the output.
    case DW_TAG_const_type:
    case DW_TAG_volatile_type:
    case DW_TAG_restrict_type:
      return getType(die, DW_AT_type);

    // Fundamental types
    case DW_TAG_base_type: {
      TypeKind kind = (TypeKind)
        die->getAttributeValueAsUnsignedConstant(unit, DW_AT_encoding,
            DW_ATE_unsigned);
      std::string encoding = "";
      switch (kind) {
      case DW_ATE_boolean: encoding = "bool"; break;
      // XXX: Represent complex float as a struct float pair?
      case DW_ATE_complex_float: encoding = "cf"; break;
      case DW_ATE_float: encoding = "f"; break;
      case DW_ATE_signed: case DW_ATE_signed_char: encoding = "s"; break;
      case DW_ATE_unsigned: case DW_ATE_unsigned_char: encoding = "u"; break;
      case DW_ATE_UTF: encoding = "u"; break;
      default:
        errs() << "Unhandled kind " << AttributeEncodingString(kind) << '\n';
        abort();
      }
      encoding += std::to_string(8 * size);
      return std::make_shared<TypeInfo>(encoding, size);
    }

    // Treat an enum as a typedef to an integer type.
    case DW_TAG_enumeration_type: {
      std::string encoding = "u" + std::to_string(8 * size);
      return std::make_shared<TypeInfo>(encoding, size);
    }

    // Pass through typedefs.
    case DW_TAG_typedef:
      return getType(die, DW_AT_type);

    // Pretend C++ classes and C structs are the same.
    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type: {
      // We may need to refer to this type later, so insert it into the struct
      // early.
      std::string name;
      raw_string_ostream(name) << "struct" <<
        format_hex_no_prefix(die->getOffset(), 8);
      std::shared_ptr<TypeInfo> type = std::make_shared<TypeInfo>(name, size);
      types[die->getOffset()] = type;

      // Add subentries for various pieces of the struct.
      const DWARFDebugInfoEntryMinimal *childDie = die->getFirstChild();
      for (; childDie && childDie->getTag(); childDie = childDie->getSibling()) {
        if (childDie->getTag() == DW_TAG_variant_part) {
          errs() << "Unhandled case, need example\n";
          die->dump(llvm::errs(), const_cast<DWARFUnit *>(unit), 1);
          abort();
        }

        // These are the only thing that actually represent data elements
        // in the struct. Well, subprograms have vtable offsets. But let's punt
        // on that until we actually need to worry about them.
        if (childDie->getTag() != DW_TAG_inheritance &&
            childDie->getTag() != DW_TAG_member) {
          continue;
        }
        uint64_t offset = childDie->getAttributeValueAsUnsignedConstant(unit,
          DW_AT_data_member_location, ~0U);
        type->getFields().emplace_back(makeType(childDie), offset);
      }
      return type;
    }
    case DW_TAG_inheritance:
    case DW_TAG_member: {
      return getType(die, DW_AT_type);
    }

    case DW_TAG_array_type: {
      std::shared_ptr<TypeInfo> inner = getType(die, DW_AT_type);
      std::string type = inner->getName();
      uint32_t size = inner->getSize();
      const DWARFDebugInfoEntryMinimal *childDie = die->getFirstChild();
      for (; childDie && childDie->getTag(); childDie = childDie->getSibling()) {
        std::shared_ptr<TypeInfo> rangeInfo = makeType(childDie);
        type += "[";
        type += rangeInfo->getName();
        type += "]";
        size *= rangeInfo->getSize();
      }
      return std::make_shared<TypeInfo>(type, size);
    }
    // This is the inner of an array type.
    case DW_TAG_subrange_type: {
      uint64_t count =
        die->getAttributeValueAsUnsignedConstant(unit, DW_AT_count, 0);
      return std::make_shared<TypeInfo>(std::to_string(count), count);
    }

    // Handle function types.
    case DW_TAG_subroutine_type: {
      std::string type = getType(die, DW_AT_type)->getName();
      // DW_AT_type isn't necessary... I guess it's void?
      if (type == "<unknown>")
        type = "void";
      char nextChar = '(';
      const DWARFDebugInfoEntryMinimal *childDie = die->getFirstChild();
      for (; childDie && childDie->getTag(); childDie = childDie->getSibling()) {
        type += nextChar;
        type += makeType(childDie)->getName();
        nextChar = ',';
      }
      type += ")";
      return std::make_shared<TypeInfo>(type, ~0u);
    }
    case DW_TAG_formal_parameter:
      return getType(die, DW_AT_type);
    case DW_TAG_unspecified_parameters:
      return std::make_shared<TypeInfo>("...", ~0u);

    case DW_TAG_ptr_to_member_type: {
      // Someone has a pointer to a member in libxul, it seems, so might as well
      // handle this. It's not correct, but let's assume Itanium C++ ABI here.
      // A type of int Foo::* is effectively a ptrdiff_t (intptr_t).
      // A type of int (Foo::*)() is effectively:
      // struct {
      //   union {
      //     int (*nonvirt)();
      //     ptrdiff_t vtblOffsetPlus1;
      //   } ptr;
      //   ptrdiff_t adj;
      // }
      std::shared_ptr<TypeInfo> pointeeType = getType(die, DW_AT_type);
      std::string name = pointeeType->getName();
      if (*name.rbegin() == ')') {
        errs() << "Pointer-to-member functions are EVIL!\n";
        abort();
      }
      name += " T::*";
      // XXX: pointersize
      return std::make_shared<TypeInfo>(name, 8);
    }

    case DW_TAG_unspecified_type: {
      // Things like void? the first one was decltype(nullptr)
      //die->dump(llvm::errs(), const_cast<DWARFUnit *>(unit), 0);
      return std::make_shared<TypeInfo>("void", ~0u);
    }

    default: {
      const char *tagString = TagString(die->getTag());
      if (!tagString) tagString = "";
      errs() << "Unhandled tag " << tagString << '\n';
      die->dump(llvm::errs(), const_cast<DWARFUnit *>(unit), 0);
      abort();
      return std::make_shared<TypeInfo>("", ~0u);
    }
  }
}

void DwarfVariableFinder::printJSON(raw_ostream &out) {
  out << "{\"locals\": [\n  ";
  out << join(locals.begin(), locals.end(), ",\n  ");
  out << "\n],\n\"globals\": [\n  ";
  out << join(globals.begin(), globals.end(), ",\n  ");
  out << "\n],\n\"types\": ";
  printTypeJSON(out);
  out << "}\n";
}

void DwarfVariableFinder::printTypeJSON(raw_ostream &out) {
  char prefix = '{';
  for (auto &entry : types) {
    auto &fields = entry.second->getFields();
    if (fields.empty())
      continue;
    out << prefix << "\n\"" << entry.second->getName() << "\":{\n";
    out.indent(2) << "\"size\":" << entry.second->getSize() << ",\n";
    out.indent(2) << "\"fields\":";
    char prefix2 = '[';
    for (auto &field : fields) {
      out << prefix2 << '\n';
      out.indent(4) << "{\"offset\":" << field.offset;
      out << ", \"size\":" << field.type->getSize();
      out << ", \"type\":\"" << field.type->getName();
      out << "\"}";
      prefix2 = ',';
    }
    out << "\n";
    out.indent(2) << "]\n";
    out << "}";
    prefix = ',';
  }
  if (prefix == '{')
    out << '{';
  out << "}\n";
}

std::string DwarfVariableFinder::parseLocationList(FunctionInfo &context,
                                                   uint64_t secOffset) {
  DataExtractor debugLoc(unit->getContext().getLocSection().Data, true,
    unit->getAddressByteSize());
  std::string listData = "";

  std::vector<std::string> entries;

  uint32_t offset = secOffset;
  while (true) {
    uint64_t startAddr = debugLoc.getAddress(&offset);
    uint64_t endAddr = debugLoc.getAddress(&offset);
    if (startAddr == 0 && endAddr == 0)
      break;
    uint32_t locExprSize = debugLoc.getU16(&offset);
    std::string locationValue = context.parseLocation(
        ArrayRef<uint8_t>((uint8_t*)debugLoc.getData().data() + offset,
          locExprSize), unit);
    entries.emplace_back("{\"start\": " + std::to_string(startAddr) +
      ", \"end\": " + std::to_string(endAddr) + ", \"location\": \"" +
      locationValue + "\"}");
    offset += locExprSize;
  }
  return "[" + join(entries.begin(), entries.end(), ",") + "]";
}

static void printTypes(const DWARFDebugInfoEntryMinimal *die, DWARFUnit *u) {
  DwarfVariableFinder finder(*u);
  finder.findVariables(die);
  finder.printJSON(outs());
}

static cl::list<std::string>
InputFilenames(cl::Positional, cl::desc("<input object files>"),
               cl::ZeroOrMore);

static int ReturnValue = EXIT_SUCCESS;

static bool error(StringRef Filename, std::error_code EC) {
  if (!EC)
    return false;
  errs() << Filename << ": " << EC.message() << "\n";
  ReturnValue = EXIT_FAILURE;
  return true;
}

static void DumpInput(StringRef Filename) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> BuffOrErr =
      MemoryBuffer::getFileOrSTDIN(Filename);
  if (error(Filename, BuffOrErr.getError()))
    return;
  std::unique_ptr<MemoryBuffer> Buff = std::move(BuffOrErr.get());

  ErrorOr<std::unique_ptr<ObjectFile>> ObjOrErr =
      ObjectFile::createObjectFile(Buff->getMemBufferRef());
  if (error(Filename, ObjOrErr.getError()))
    return;
  ObjectFile &Obj = *ObjOrErr.get();
  // Save the triple for the object (XXX: bad architecture).
  getTarget(&Obj);

  std::unique_ptr<DWARFContext> DICtx(new DWARFContextInMemory(Obj));

  for (auto &compileUnit : DICtx->compile_units())
    printTypes(compileUnit->getUnitDIE(false), compileUnit.get());
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;
  InitializeAllTargetInfos();
  InitializeAllTargetMCs();

  cl::ParseCommandLineOptions(argc, argv, "DWARF debug info dumper\n");

  // Defaults to a.out if no filenames specified.
  if (InputFilenames.size() == 0)
    InputFilenames.push_back("a.out");

  std::for_each(InputFilenames.begin(), InputFilenames.end(), DumpInput);

  return ReturnValue;
}
