#include <iostream>

#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"

/* Simulate the program link.

   Lists any modules that cannot be found/linked into the psuedo target.
   Afer completion lists which procedure names are inside the target and
   which are left dangling as external references. This can be used to
   find out which symbols are in external libraries and/or missing because
   of module linking problems.
*/
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "usage: modlist <IR files>...\n";
        return 1;
    }
    llvm::SMDiagnostic Diag;
    llvm::LLVMContext C;
    llvm::Module WP("wholeprogram",C);
    llvm::Linker linker(WP);

    for(int i=1; i<argc; i++) {
        std::unique_ptr<llvm::Module> M = llvm::parseIRFile(argv[i], Diag, C);
        bool broken_debug_info = false;
        if (M.get() == nullptr || llvm::verifyModule(*M, &llvm::errs(), &broken_debug_info)) {
            llvm::errs() << "error: module " << argv[i] << " not valid\n";
            continue;
        }
        if (broken_debug_info) {
            llvm::errs() << "caution: debug info is broken\n";
            continue;
        }
        linker.linkInModule(std::move(M));
    }

    for (auto &F: WP.functions()) {
        if (F.isDeclaration())
            std::cout << "Outside " << F.getName().str() << " " << F.getLinkage() << std::endl;
    }
    for (auto &F: WP.functions()) {
        if (!F.isDeclaration())
            std::cout << "Inside " << F.getName().str() << " " << F.getLinkage() << std::endl;
    }
    llvm::llvm_shutdown();
}
