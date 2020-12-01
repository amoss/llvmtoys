#include <iostream>
//#include <set>
//#include <string>

#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
//#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/GraphWriter.h"
//#include "llvm/Support/raw_ostream.h"
//#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LegacyPassNameParser.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"

// Could use llvm::ValueLatticeElement but it looks far more complex
class MemberValue {
    int offset, size;
    bool top, bottom, constant;
    llvm::Constant *value;
};

class RegionValue {
    int size;
    bool bottom;
    llvm::Value *value;
    std::list<std::unique_ptr<MemberValue>> contents;
    std::map<int,MemberValue*> offsets;

    public:
    RegionValue(int s, llvm::Value *v) : size(s), bottom(false), value(v) {}


};

llvm::Function *findFunction(llvm::Module &WP, const char *fname) {
    for (auto &F: WP.functions())
    {
        std::string name = F.getName().str();
        if (name.compare(fname)==0)
            return &F;
    }
    return NULL;
}

void analyseFunction(llvm::Function *F) { //, RegionValue &rv) {
    llvm::outs() << "Analysis of " << F->getName() << "\n";
    std::set<llvm::Value*> values;
    std::set<llvm::Instruction*> insts;
    for (auto arg = F->arg_begin(), end=F->arg_end(); arg!=end; ++arg) {
        values.insert(arg);
    }
    for (auto &BB: *F) {
        for (auto &I: BB) {
            //llvm::outs() << I << "\n";
            if (!I.getType()->isVoidTy() )
                values.insert(&I);
            insts.insert(&I);
            for (auto U = I.op_begin(), end = I.op_end(); U!=end; ++U) {
                if (llvm::dyn_cast<llvm::Constant>(U) == nullptr) {
                    values.insert(U->get());
                }
            }
        }
    }
    for ( llvm::Value *v: values) {
        llvm::outs() << "v" << v << " [label=\"";
        v->printAsOperand( llvm::outs() );
        llvm::outs() << "\"];\n";
    }
    for ( llvm::Instruction *i: insts) {
        llvm::outs() << "i" << i << " [label=\"" << i->getOpcodeName() << "\"];\n";
    }
    for (auto &BB: *F) {
        for (auto &I: BB) {
            if (!I.getType()->isVoidTy() ) {
                llvm::outs() << "i" << &I << " -> " << "v" << &I << ";\n";
            }
            for (auto U = I.op_begin(), end = I.op_end(); U!=end; ++U) {
                if (llvm::dyn_cast<llvm::Constant>(U) == nullptr) {
                    llvm::outs() << "v" << U->get() << " -> i" << &I << ";\n";
                }
            }
        }
    }
}


void freshRegions(llvm::Module &WP) {
    llvm::Function *malloc = findFunction(WP, "malloc");
    if (!malloc)  return;

    for( llvm::User *U : malloc->users() ) {
        if (auto *CI = llvm::dyn_cast<llvm::CallBase>(U)) {
            if (CI->getCalledFunction() == malloc) { // Calling, not using address-of
                auto *caller = CI->getFunction();
                llvm::outs() << "Fresh in " << caller->getName() << ": " << *CI << "\n";
                std::string Filename = "fresh." + caller->getName().str() + ".dot";
                std::error_code ErrorInfo;
                llvm::raw_fd_ostream File(Filename.c_str(), ErrorInfo);
                llvm::DOTFuncInfo info(caller);
                if (ErrorInfo)
                    llvm::errs() << "Can't open graph! " << ErrorInfo.value() << "\n";
                else
                    llvm::WriteGraph(File, &info, false);
                llvm::Value *sizeArg = CI->getArgOperand(0);
                llvm::ConstantInt *sizeConst = llvm::dyn_cast<llvm::ConstantInt>(sizeArg);
                analyseFunction(caller); //, rv);
                if (sizeConst) {
                    RegionValue rv(sizeConst->getValue().getLimitedValue(), llvm::dyn_cast<llvm::Value>(U) );
                } else {
                    // Need to walk back up chain to see if the size can be determined
                }
            }
        }
    }
}


/*void outputCallSites(llvm::Module &WP, char *fname) {
    for (auto &F: WP.functions())
    {
        std::string name = F.getName().str();
        if (name.compare(fname)!=0)
            continue;
        for(llvm::User *U : F.users()) {
            llvm::outs() << fname << " in " << *U << "\n";
            if (auto *CI = llvm::dyn_cast<llvm::CallBase>(U)) {
                if (CI->getCalledFunction() != &F)

                    llvm::outs() << name << " in: " << *CI << "\n";
            } else
            if (auto *I = llvm::dyn_cast<llvm::Instruction>(U)) {
                llvm::outs() << name << " in: " << *I << "\n";
            } else {
                llvm::outs() << *U << "\n";
            }
        }
    }
}*/

// Rough plan:
// * Understand the SCCP pass code
// * Write a new pass
//   * For a target function, split call-sites into those with constant values and those without
//   * Rewrite the call-sites to variants of the function...

// What we are trying to do:
// * Find calls that land in the target function (create_thread)
// * Find call-site with constant args (eve if several levels back)
// * Add missing edges to call-graph

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

    llvm::outs() << "Before:\n";
    //outputCallSites(WP, argv[1]);

    llvm::legacy::PassManager MPM;
    llvm::legacy::FunctionPassManager FPM(&WP);
    llvm::PassManagerBuilder Builder;

    Builder.OptLevel = 2;
    Builder.DisableUnrollLoops = false;
    Builder.populateFunctionPassManager(FPM);
    Builder.populateModulePassManager(MPM);
    Builder.Inliner = llvm::createFunctionInliningPass();

    MPM.run(WP);

    llvm::outs() << "After:\n";
    //outputCallSites(WP, argv[1]);
    freshRegions(WP);
    llvm::llvm_shutdown();
}
