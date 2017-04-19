#include "utils.h"

size_t HANDLE_DW_ATE_SIZE = 19;
const char* HANDLE_DW_ATE[19] = {
  "void", 
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
  "ASCII"
};



DwarfVariableFinder::DwarfVariableFinder(const DWARFDie &die, raw_ostream &os) : OS(os) {
  if(!die.hasChildren()) {
    outs() << "No child \n\n";
    return;
  }

  for (auto child = die.getFirstChild(); child; child = child.getSibling()) {
    //Go over all the top level sub_programs
    if(child.isSubprogramDIE() || child.isSubroutineDIE()) {
      getInfo(child);

      //Look for variables among children of sub_program die
      if(!child.hasChildren()) {
        continue;
      }
      findVariables(child);
    }
  }
}


void DwarfVariableFinder::findVariables(const DWARFDie &die) {

  for (auto child = die.getFirstChild(); child; child = child.getSibling()) {
    switch(child.getTag()) {
      case dwarf::DW_TAG_variable:
      case dwarf::DW_TAG_formal_parameter:
      case dwarf::DW_TAG_constant:
        handleVariable(child);
        break;
      default:
        if (child.hasChildren())
          findVariables(child);
    }
  }
}

void DwarfVariableFinder::handleVariable(const DWARFDie &die) {
  //die.dump(outs(), 10);
  getInfo(die);
  auto type = getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type));
  type->dump(outs());

}

void DwarfVariableFinder::getInfo(const DWARFDie &die) {
  auto tagString = TagString(die.getTag());
  if (tagString.empty()) {
    outs() << format("DW_TAG_Unknown_%x", die.getTag());
  }
  auto formVal = die.find(dwarf::DW_AT_name);
  formVal->dump(outs());
}

std::shared_ptr<TypeInfo> DwarfVariableFinder::getType(const DWARFDie &die) {

  if (!die.isValid()) {
    assert(0);
    return std::make_shared<TypeInfo>("", ~0u);
  }

  auto die_offset = die.getOffset();  
  if(typeDict.count(die_offset)) {
    return typeDict[die_offset];
  }

  auto result = makeType(die);
  typeDict[die_offset] = result;
  return result;
}

std::shared_ptr<TypeInfo> DwarfVariableFinder::makeType(const DWARFDie &die) {

  if (!die.isValid()) {
    return std::make_shared<TypeInfo>("", ~0u);
  }
  auto opSize = die.find(dwarf::DW_AT_byte_size);
  unsigned size = 1;
  if(opSize.hasValue()) {
    size = opSize.getValue().getAsUnsignedConstant().getValue();  
  }

  std::string type_encoding = "";
  raw_string_ostream SS(type_encoding);

  switch (die.getTag()) {
    case dwarf::DW_TAG_base_type: {  
                                    auto opForm = die.find(dwarf::DW_AT_encoding);
                                    auto opEnc = opForm->getAsUnsignedConstant();
                                    assert(opEnc < HANDLE_DW_ATE_SIZE);
                                    SS << HANDLE_DW_ATE[*opEnc];
                                    opForm = die.find(dwarf::DW_AT_name);
                                    opForm->dump(SS);
                                    return std::make_shared<TypeInfo>(SS.str(), size);
                                  }
    case dwarf::DW_TAG_reference_type:
    case dwarf::DW_TAG_rvalue_reference_type:
    case dwarf::DW_TAG_pointer_type: {
                                       auto baseType = getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type));
                                       SS <<  "*" << baseType->getName();
                                       return std::make_shared<TypeInfo>(SS.str(), size);
                                     }
    case dwarf::DW_TAG_array_type: {
                                     auto baseType = getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type));
                                     SS << baseType->getName();
                                     size *= baseType->getSize();

                                     for (auto childDie = die.getFirstChild(); childDie && childDie.getTag(); 
                                         childDie = childDie.getSibling()) {
                                       std::shared_ptr<TypeInfo> rangeInfo = makeType(childDie);
                                       SS << "[";
                                       SS <<  rangeInfo->getName();
                                       SS << "]";
                                       size *= rangeInfo->getSize();
                                     }
                                     return std::make_shared<TypeInfo>(SS.str(), size);
                                   }
    case dwarf::DW_TAG_subrange_type: {
                                        uint64_t count = 0;
                                        auto opCount = die.find(dwarf::DW_AT_count);
                                        if(opCount.hasValue()) {
                                          count = opCount.getValue().getAsUnsignedConstant().getValue();
                                        } else {
                                          opCount = die.find(dwarf::DW_AT_upper_bound);
                                          assert(opCount.hasValue());
                                          count = opCount.getValue().getAsUnsignedConstant().getValue()  +1;
                                        }
                                        return std::make_shared<TypeInfo>(std::to_string(count), count);
                                      }
    case dwarf::DW_TAG_typedef: {
                                  return getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type));
                                }

    case dwarf::DW_TAG_structure_type:
    case dwarf::DW_TAG_class_type:
    case dwarf::DW_TAG_union_type: {
                                     SS << "struct" << dwarf::toString(die.find(dwarf::DW_AT_name), "None");
                                     auto structType = std::make_shared<TypeInfo>(SS.str(), size);

                                     // Add subentries for various pieces of the struct.
                                     for (auto childDie = die.getFirstChild(); childDie && childDie.getTag(); childDie = childDie.getSibling()) {
                                       if (childDie.getTag() != dwarf::DW_TAG_inheritance &&
                                           childDie.getTag() != dwarf::DW_TAG_member) {
                                         continue;
                                       }
                                       uint64_t dataMemOffset = dwarf::toUnsigned(childDie.find(dwarf::DW_AT_data_member_location), ~0U);
                                       structType->getFields().emplace_back(makeType(childDie), dataMemOffset);
                                     }
                                     return structType;
                                   }

    case dwarf::DW_TAG_inheritance:
    case dwarf::DW_TAG_member: {
                                 return getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type));
                               }        

    default: {
               auto tagString = TagString(die.getTag());
               if (tagString.empty()) {
                 llvm::errs() << format("DW_TAG_Unknown_%x", die.getTag());
               }
               die.dump(llvm::errs(), 10);
               return std::make_shared<TypeInfo>("", ~0u);
             }
  }
}

void DwarfVariableFinder::dump() {
  OS << "\n\nSummary\n";
  for(auto item: typeDict) {
    OS << "Die Offset: " << item.first << "\n\t";
    item.second->dump(OS) ;
  }
}

void TypeInfo::dump(raw_ostream &OS) {
  OS << "Name: " << name << " Size: " << size << " ";
  for(auto field : fields) {
    OS << "\t";
    field.type->dump(OS);
    OS << "Member Offset: " << field.offset << "\n";
  }
  OS << "\n";
}




