#include "utils.h"

DwarfVariableFinder::DwarfVariableFinder(const DWARFDie &die) {
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
  getInfo(die);
  getType(die.getAttributeValueAsReferencedDie(dwarf::DW_AT_type));
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
    return std::make_shared<TypeInfo>("", ~0u);
}




