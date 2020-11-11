#include <iostream>
#include <set>
#include <string>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"

void callPathsDFS(llvm::CallGraphNode* node, std::vector<std::string> &path,
                  std::set<llvm::CallGraphNode*> &visited) {
    visited.insert(node);
    llvm::Function *F = node->getFunction();
    if (F && F->hasName()) {
        path.push_back(F->getName().str());
        llvm::outs() << "Visiting ";
        for(auto P: path)
            llvm::outs() << P << " ";
        llvm::outs() << "\n";
    }
    for (auto called: *node) {
        if (visited.find(called.second) == visited.end()) {
            callPathsDFS(called.second, path, visited);
        }
    }
    if (F && F->hasName())
        path.pop_back();
}

void callPaths(llvm::CallGraph &cg) {
    std::set<llvm::CallGraphNode*> visited;
    auto *entry = cg.getExternalCallingNode();
    std::vector<std::string> path;
    callPathsDFS(entry, path, visited);
}


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
            llvm::errs() << "error: module not valid\n";
            return 1;
        }
        if (broken_debug_info) {
            llvm::errs() << "caution: debug info is broken\n";
            return 1;
        }
        linker.linkInModule(std::move(M));
    }

    llvm::CallGraph cg(WP);
    //cg.print(llvm::outs());
    callPaths(cg);




    llvm::llvm_shutdown();
}

