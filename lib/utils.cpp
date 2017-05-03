#include "utils.h"
#define DEBUG_TYPE "dwarf_type_reader"


size_t HANDLE_DW_ATE_SIZE = 19;
const char *HANDLE_DW_ATE[19] = {"void",
                                 "address",
                                 "boolean",
                                 "complex_float",
                                 "float",
                                 "signed",
                                 "signed_char",
                                 "unsigned",
                                 "unsigned_char",
                                 "imaginary_float",
                                 "packed_decimal",
                                 "numeric_string",
                                 "edited",
                                 "signed_fixed",
                                 "unsigned_fixed",
                                 "decimal_float",
                                 "UTF",
                                 "UCS",
                                 "ASCII"};

void error(StringRef Filename, std::error_code EC) {
  if (!EC)
    return;
  errs() << Filename << ": " << EC.message() << "\n";
  exit(1);
}

/*
 * Purpose: Each instance of  DwarfVariableFinder is responsible to find
 *          all the variables in each of the CU's in a particular 'Filename'
 */
DwarfVariableFinder::DwarfVariableFinder(StringRef Filename) {
  std::string outFile = Filename.str() + ".debuginfo";
  std::error_code EC;

  OS = new std::fstream(outFile,
                        std::ios::out | std::ios::trunc | std::ios::binary);
  error(outFile, EC);
}

DwarfVariableFinder::~DwarfVariableFinder() { OS->close(); }

/*
 * Purpose: Find all the variables (locals & gobals) inside the compilation unit
 * die 'die'
 */
void DwarfVariableFinder::findVariablesInCU(const DWARFDie &CU) {

  for (auto child = CU.getFirstChild(); child; child = child.getSibling()) {
    // Go over all the top level sub_programs
    if (child.isSubprogramDIE() || child.isSubroutineDIE()) {
      // getInfo(child);

      // Look for variables among children of sub_program die
      if (!child.hasChildren()) {
        continue;
      }
      findVariablesInScope(child);
    }
  }
}

/*
 * Purpose: Find all the variables in the scope die. scope die could be a
 * subroutine or compilation unit die.
 */
void DwarfVariableFinder::findVariablesInScope(const DWARFDie &scope_die) {
  for (auto child = scope_die.getFirstChild(); child;
       child = child.getSibling()) {
    switch (child.getTag()) {
    case dwarf::DW_TAG_variable:
    case dwarf::DW_TAG_formal_parameter:
    case dwarf::DW_TAG_constant: {
      ::VariableType::StackVar *LV = Vars.add_stack_variables();
      DEBUG(
      llvm::errs() << "Var Die : \n";
      child.dump(llvm::errs(), 10);
      );

      if (child.getTag() == dwarf::DW_TAG_formal_parameter) {
        LV->set_is_formal_parameter(true);
      } else {
        LV->set_is_formal_parameter(false);
      }

      auto *scope = LV->mutable_scope();
      scope->set_symbol_name(
          dwarf::toString(scope_die.find(dwarf::DW_AT_name), "None"));

      uint64_t LowPC, HighPC;
      LowPC = HighPC = ~0U;
      assert(scope_die.getLowAndHighPC(LowPC, HighPC));
      scope->set_entry_address(LowPC);

      auto *var = LV->mutable_var();
      var->set_name(dwarf::toString(child.find(dwarf::DW_AT_name), "None"));

      auto *TY = var->mutable_type();
      auto type = getType(
          child.getAttributeValueAsReferencedDie(dwarf::DW_AT_type), TY);

      break;
    }
    default:
      if (child.hasChildren())
        findVariablesInScope(child);
      break;
    }
  }
}

std::shared_ptr<::VariableType::Type>
DwarfVariableFinder::getType(const DWARFDie &die, ::VariableType::Type *TY) {

  DEBUG(
    llvm::errs() << "At Entry : \n";
    die.dump(llvm::errs(), 10);
  );
  if (!die.isValid()) {
    llvm::errs() << "Problematic die: \n";
    die.dump(llvm::errs(), 10);
    assert(0 && "Invalid Die");
    return std::make_shared<::VariableType::Type>(::VariableType::Type());
  }

  auto die_offset = die.getOffset();
  if (typeDict.count(die_offset)) {
    *TY = *(typeDict[die_offset]);
    return typeDict[die_offset];
  }

  auto result = makeType(die, TY);
  typeDict[die_offset] = result;
  return result;
}

std::shared_ptr<::VariableType::Type>
DwarfVariableFinder::makeType(const DWARFDie &die, ::VariableType::Type *TY) {

  if (!die.isValid()) {
    assert(0 && "Invalid Die");
    return std::make_shared<::VariableType::Type>(::VariableType::Type());
  }

  // For DW_TAG_pointer_type, we do not have the size
  TY->set_size(dwarf::toUnsigned(
          die.find(dwarf::DW_AT_byte_size), ~0U));

  std::string type_encoding = "";
  raw_string_ostream SS(type_encoding);

  switch (die.getTag()) {
  case dwarf::DW_TAG_base_type: {
    auto opForm = die.find(dwarf::DW_AT_encoding);
    auto opEnc = opForm->getAsUnsignedConstant();
    assert(opEnc < HANDLE_DW_ATE_SIZE);
    TY->set_c_type(std::string(HANDLE_DW_ATE[*opEnc]));
    TY->set_kind(::VariableType::Type::isScalar);
    return std::make_shared<::VariableType::Type>(*TY);
  }
  case dwarf::DW_TAG_reference_type:
  case dwarf::DW_TAG_rvalue_reference_type:
  case dwarf::DW_TAG_pointer_type: {
    TY->set_kind(::VariableType::Type::isPointer);
    auto *ETY = TY->mutable_element_type();

    auto baseTypeDie = die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type);
    if(!baseTypeDie.isValid()) {
      // Handle void type
      ETY->set_c_type("void");
      ETY->set_size(0);
      ETY->set_kind(::VariableType::Type::isScalar);
    } else {
       getType(baseTypeDie, ETY);
    }
    TY->set_c_type("* " + ETY->c_type());
    return std::make_shared<::VariableType::Type>(*TY);
  }
  case dwarf::DW_TAG_array_type: {
    TY->set_kind(::VariableType::Type::isArray);
    auto *ETY = TY->mutable_element_type();
    auto size = 1;

    auto baseType =
        getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type), ETY);
    size *= ETY->size();
    auto c_type = ETY->c_type();

    for (auto childDie = die.getFirstChild(); childDie && childDie.getTag();
         childDie = childDie.getSibling()) {
      auto *DTY = new ::VariableType::Type();
      makeType(childDie, DTY);
      c_type = c_type + "[" + DTY->c_type() + "]";
      size *= DTY->size();
    }
    TY->set_size(size);
    TY->set_c_type(c_type);
    return std::make_shared<::VariableType::Type>(*TY);
  }
  case dwarf::DW_TAG_subrange_type: {
    uint64_t count = 0;
    auto opCount = die.find(dwarf::DW_AT_count);
    if (opCount.hasValue()) {
      count = opCount.getValue().getAsUnsignedConstant().getValue();
    } else {
      opCount = die.find(dwarf::DW_AT_upper_bound);
      if(!opCount.hasValue()) {
        //llvm::errs() << "Problematic die: \n";
        //die.dump(llvm::errs(), 10);
        llvm::errs() << "dwarf::DW_TAG_subrange_type uppper bound missing: May be a flexible array\n";
        //assert(0 && "dwarf::DW_TAG_subrange_type uppper bound missing: May be a flexible array");
        count = 0;
      } else {
        count = opCount.getValue().getAsUnsignedConstant().getValue() + 1;
      }
    }
    TY->set_c_type(std::to_string(count));
    TY->set_size(count);
    return std::make_shared<::VariableType::Type>(*TY);
  }
  case dwarf::DW_TAG_typedef: {
    auto baseTypeDie = die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type);
    if(!baseTypeDie.isValid()) {
      // Handle void type
      TY->set_c_type("void");
      TY->set_size(0);
      TY->set_kind(::VariableType::Type::isScalar);
      return std::make_shared<::VariableType::Type>(*TY);
    } 
    return getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type), TY);
  }
  case dwarf::DW_TAG_structure_type:
  case dwarf::DW_TAG_class_type:
  case dwarf::DW_TAG_union_type: {
    TY->set_kind(::VariableType::Type::isStruct);
    TY->set_c_type(std::string("struct ") +
                   dwarf::toString(die.find(dwarf::DW_AT_name), "None"));
    typeDict[die.getOffset()] = std::make_shared<::VariableType::Type>(*TY);

    // Add subentries for various pieces of the struct.
    for (auto childDie = die.getFirstChild(); childDie && childDie.getTag();
         childDie = childDie.getSibling()) {
      if (childDie.getTag() != dwarf::DW_TAG_inheritance &&
          childDie.getTag() != dwarf::DW_TAG_member) {
        continue;
      }
      auto *field = TY->add_member_list();
      field->set_field_offset(dwarf::toUnsigned(
          childDie.find(dwarf::DW_AT_data_member_location), ~0U));
      field->set_field_name(
          dwarf::toString(childDie.find(dwarf::DW_AT_name), "None"));
      auto *FTY = field->mutable_field_type();
      makeType(childDie, FTY);
    }
    return std::make_shared<::VariableType::Type>(*TY);
    //return typeDict[die.getOffset()];
  }

  case dwarf::DW_TAG_inheritance:
  case dwarf::DW_TAG_member: {
    return getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type), TY);
  }

  default: {
    auto tagString = TagString(die.getTag());
    if (tagString.empty()) {
      llvm::errs() << format("DW_TAG_Unknown_%x", die.getTag());
    }
    die.dump(llvm::errs(), 10);
    return std::make_shared<::VariableType::Type>(::VariableType::Type());
  }
  }
}

/*
 * Purpose: Dump the collected variable dwarf into to protobuff binary format
 */
void DwarfVariableFinder::dump() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (!Vars.SerializeToOstream(OS)) {
    assert(0 && "Failed to write");
  }
  DEBUG(
    llvm::errs() << Vars.DebugString();
  );
  google::protobuf::ShutdownProtobufLibrary();
}
