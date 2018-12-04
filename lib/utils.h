#ifndef UTILS_H
#define UTILS_H

#include "variable_type.pb.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/DebugInfo/DWARF/DWARFDie.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include <fstream>

using namespace llvm;

class TypeInfo;
class DwarfVariableFinder final {
public:
  DwarfVariableFinder(StringRef);
  ~DwarfVariableFinder();
  void findVariablesInCU(const DWARFDie &, uint64_t);
  void findVariablesInScope(const DWARFDie &scope);
  std::shared_ptr<::VariableType::VarType> getType(const DWARFDie &die,
                                                ::VariableType::VarType *);
  std::shared_ptr<::VariableType::VarType> makeType(const DWARFDie &die,
                                                 ::VariableType::VarType *);
  void dump();

  // Comes from protobuff definition
  ::VariableType::Variables Vars;

private:
  // Map: Dwaft type die offset --> Type Info
  DenseMap<uint32_t, std::shared_ptr<::VariableType::VarType>> typeDict;
  std::fstream *OS;
};

void error(StringRef Filename, std::error_code EC);

#endif
