#include <iostream>
//#include <set>
//#include <string>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
//#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/ManagedStatic.h"
//#include "llvm/Support/raw_ostream.h"
//#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "usage: constcalls <funcname> <IR files>...\n";
        return 1;
    }
    llvm::SMDiagnostic Diag;
    llvm::LLVMContext C;
    llvm::Module WP("wholeprogram",C);
    llvm::Linker linker(WP);

    for(int i=2; i<argc; i++) {
        std::unique_ptr<llvm::Module> M = llvm::parseIRFile(argv[i], Diag, C);
        bool broken_debug_info = false;
        if (M.get() == nullptr || llvm::verifyModule(*M, &llvm::errs(), &broken_debug_info)) {
            llvm::errs() << "error: module not valid\n";
            return 1;
        }
        if (broken_debug_info) {
            llvm::errs() << "caution: debug info is broken\n";
            return 1;
        }
        linker.linkInModule(std::move(M));
    }

    for (auto &F: WP.functions())
    {
        std::string name = F.getName().str();
        if (name.compare(argv[1])!=0)
            continue;
        llvm::outs() << name << "\n";
        for(llvm::User *U : F.users()) {
            if (auto *I = llvm::dyn_cast<llvm::Instruction>(U)) {
                // if call instruction
                //  ... ->getCalledFunction() doesn't match then skip
                // otherwise keep references
                llvm::outs() << "Inst " << *I << "\n";
            } else {
                llvm::outs() << *U << "\n";
            }
        }
    }
    llvm::llvm_shutdown();
}
