#ifndef UTILS_H
#define UTILS_H

#include "llvm/DebugInfo/DWARF/DWARFDie.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/raw_ostream.h"
#include "variable_type.pb.h"
#include <fstream>

using namespace llvm;

class TypeInfo;
class DwarfVariableFinder final {
  public:
    DwarfVariableFinder(StringRef);
    ~DwarfVariableFinder();
    void findVariablesInCU(const DWARFDie &CU);
    void findVariablesInScope(const DWARFDie &scope);
    std::shared_ptr<::VariableType::Type> getType(const DWARFDie &die, ::VariableType::Type *);
    std::shared_ptr<::VariableType::Type> makeType(const DWARFDie &die, ::VariableType::Type *);
    void dump();

    // Comes from protobuff definition
    ::VariableType::Variables Vars;
  private:
    // Map: Dwaft type die offset --> Type Info 
    DenseMap<uint32_t, std::shared_ptr<::VariableType::Type>> typeDict;
    std::fstream *OS;
};

#endif
