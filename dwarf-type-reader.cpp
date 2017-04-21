#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFCompileUnit.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFDie.h"
#include "llvm/DebugInfo/DWARF/DWARFFormValue.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"

#include "utils.h"
#include <algorithm>
#include <list>
#include <string>
#include <system_error>

using namespace llvm;
using namespace object;

static cl::list<std::string> InputFilenames(cl::Positional,
                                            cl::desc("<input object files"),
                                            cl::ZeroOrMore);

static void DumpObjectFile(ObjectFile &Obj, StringRef Filename) {
  std::unique_ptr<DWARFContext> DICtx(new DWARFContextInMemory(Obj));

  DwarfVariableFinder finder(Filename);
  for (const auto &CU : DICtx->compile_units()) {
    const DWARFDie &cu_die = CU->getUnitDIE(false);
    finder.findVariablesInCU(cu_die);
  }
  finder.dump();
}

static void DumpInput(StringRef Filename) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> BuffOrErr =
      MemoryBuffer::getFileOrSTDIN(Filename);
  error(Filename, BuffOrErr.getError());
  std::unique_ptr<MemoryBuffer> Buff = std::move(BuffOrErr.get());

  Expected<std::unique_ptr<Binary>> BinOrErr =
      object::createBinary(Buff->getMemBufferRef());
  if (!BinOrErr)
    error(Filename, errorToErrorCode(BinOrErr.takeError()));

  if (auto *Obj = dyn_cast<ObjectFile>(BinOrErr->get())) {
    DumpObjectFile(*Obj, Filename);
  }
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(argc, argv, "llvm dwarf dumper\n");

  // Defaults to a.out if no filenames specified.
  if (InputFilenames.size() == 0)
    InputFilenames.push_back("a.out");

  std::for_each(InputFilenames.begin(), InputFilenames.end(), DumpInput);

  return EXIT_SUCCESS;
}
