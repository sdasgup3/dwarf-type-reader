#ifndef UTILS__H
#define UTILS_H

#include "llvm/DebugInfo/DWARF/DWARFDie.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

class TypeInfo;
class DwarfVariableFinder final {
  public:
    DwarfVariableFinder(const DWARFDie &die, raw_ostream &OS);
    void findVariables(const DWARFDie &die);
    void handleVariable(const DWARFDie &die);
    void getInfo(const DWARFDie &die);
    std::shared_ptr<TypeInfo> getType(const DWARFDie &die);
    std::shared_ptr<TypeInfo> makeType(const DWARFDie &die);
    void dump();
  private:
    DenseMap<uint32_t, std::shared_ptr<TypeInfo>> typeDict;
    raw_ostream &OS;
};



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
    void dump(raw_ostream &OS);

  private:
    std::string name;
    uint64_t size;
    uint64_t dimention;
    std::vector<FieldInfo> fields;
};

#endif
