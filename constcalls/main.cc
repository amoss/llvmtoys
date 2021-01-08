#include <iostream>
//#include <set>
//#include <string>


#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
//#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LegacyPassNameParser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/CFG.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/ManagedStatic.h"
//#include "llvm/Support/raw_os_ostream.h"
//#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "eval.h"

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

bool constRelativeToCall(llvm::Value *v) {
    llvm::Instruction *i = llvm::dyn_cast<llvm::Instruction>(v);
    if (i==nullptr) {
        if (v->getType()->isPointerTy()) {
            llvm::outs() << "Pointer (false) <- " << *v << "\n";
            return false;
        }
        if (llvm::dyn_cast<llvm::Constant>(v) != nullptr) {
            llvm::outs() << "Const (true) <- " << *v << "\n";
            return true;
        }
        if (llvm::dyn_cast<llvm::MemoryAccess>(v) != nullptr) {
            llvm::outs() << "Mem (false) <- " << *v << "\n";
            return false;
        }
        return true;
    } else {
        switch (i->getOpcode()) {
            case llvm::Instruction::Load:
            case llvm::Instruction::Call:
                return false;
            default:
                for (auto U = i->op_begin(), end = i->op_end(); U!=end; ++U) {
                    if (!constRelativeToCall(*U))
                        return false;
                }
                return true;
        }
    }
}


typedef std::set<llvm::Value*> ReachSet;
typedef std::map<llvm::Value*,std::unique_ptr<ReachSet>> ReachGraph;

struct FunctionInfo {
    ReachGraph data, prunedCtrl;
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


void insert(ReachGraph &g, llvm::Value *src, llvm::Value *dst) {
    if (g.find(src) == g.end()) {
        g[src] = std::make_unique<ReachSet>();
    }
    if (g.find(dst) == g.end()) {
        g[dst] = std::make_unique<ReachSet>();
    }
    g[src]->insert(dst);
}

bool reaches(ReachGraph &g, llvm::Value *src, llvm::Value *dst) {
    if (g.find(src) == g.end())
        return false;
    if (g[src]->find(dst) != g[src]->end())
        return true;
    return false;
}

std::unique_ptr<FunctionInfo> analyseFunction(llvm::Function *F/*, RegionValue &rv*/) {

    auto result = std::make_unique<FunctionInfo>();

    // Bootstrap the adjacency matrix for data values in the function
    // TODO: This may invert edge direction for gloabls in stores
    for (auto &BB : *F) {
        for (auto &I: BB) {
            for (auto U = I.op_begin(), end = I.op_end(); U!=end; ++U) {
                if (llvm::dyn_cast<llvm::Constant>(U) == nullptr) {
                    //llvm::outs() << "Boot " << U->get() << " " << &I << "\n";
                    insert( result->data, U->get(), &I);
                }
            }
        }
    }

    // Take powers of the adjacency matrix until we reach a fixed point
    size_t expansion;
    do {
        expansion = 0;
        for (auto &P: result->data) {
            size_t orig = P.second->size();
            ReachSet copy(P.second->begin(), P.second->end());
            for (auto *dst: copy) {
                P.second->insert( result->data[dst]->begin(), result->data[dst]->end() );
            }
            expansion += P.second->size() - orig;
        }
    } while (expansion>0);

    // Prune the control-flow graph by dropping edges that are already covered
    // by paths in the data-flow.
    for (auto &BB: *F) {
        for (auto &I: BB) {
            if (llvm::isa<llvm::BranchInst>(I))
                continue;
            auto *prev = static_cast<llvm::Value*>(I.getPrevNonDebugInstruction());
            if (prev != nullptr && !reaches(result->data, prev, &I)) {
                insert(result->prunedCtrl, prev, &I);
            }
        }
        llvm::Instruction *psuedoLast = BB.getTerminator()->getPrevNonDebugInstruction();
        if (psuedoLast != nullptr) {
            for(auto *succ: llvm::successors(&BB)) {
                llvm::Instruction *head = &(*(succ->begin()));
                insert(result->prunedCtrl, psuedoLast, head);
            }
        }
    }

    /*for (auto &P: result->data) {
        llvm::outs() << P.first << " " << *P.first << "\n";
        for (auto *dst: *P.second.get()) {
            llvm::outs() << "  -> " << dst << " " << *dst << "\n";
        }
    }*/
    return result;
}

void functionFlowGraph(llvm::Function *F, FunctionInfo *fi) {
    std::error_code ec;
    std::string fn = "fg." + F->getName().str() + ".dot";
    llvm::raw_fd_ostream out(fn,ec);
    if (ec) return;

    out << "digraph " << F->getName().str() << " {\n";

    std::set<llvm::Value*> values;
    std::set<llvm::Instruction*> insts;
    for (auto arg = F->arg_begin(), end=F->arg_end(); arg!=end; ++arg) {
        values.insert(arg);
    }
    for (auto &BB: *F) {
        for (auto &I: BB) {
            if (llvm::isa<llvm::BranchInst>(I))
                continue;
            if (!I.getType()->isVoidTy() )
                values.insert(&I);
            insts.insert(&I);
            for (auto U = I.op_begin(), end = I.op_end(); U!=end; ++U) {
                if (llvm::dyn_cast<llvm::Constant>(U) == nullptr ) {
                    values.insert(U->get());
                }
            }
        }
    }
    for ( llvm::Value *v: values) {
        out << "v" << v << " [label=\"";
        v->printAsOperand( out );
        out << "\"];\n";
    }
    for ( llvm::Instruction *i: insts) {
        auto *cb = llvm::dyn_cast<llvm::CallBase>(i);
        if (cb != nullptr) {
            out << "i" << cb << " [shape=none,label=\"" << cb->getOpcodeName() << " "
                         << cb->getCalledFunction()->getName() << "\"];\n";
        }
        else
            out << "i" << i << " [shape=none,label=\"" << i->getOpcodeName() << "\"];\n";
    }
    for (auto &BB: *F) {
        for (auto &I: BB) {
            if (llvm::isa<llvm::BranchInst>(I))
                continue;
            if (!I.getType()->isVoidTy() ) {
                out << "i" << &I << " -> " << "v" << &I << ";\n";
            }
            for (auto U = I.op_begin(), end = I.op_end(); U!=end; ++U) {
                if (llvm::dyn_cast<llvm::Constant>(U) == nullptr) {
                    out << "v" << U->get() << " -> i" << &I << ";\n";
                }
            }
            //auto *prev = I.getPrevNonDebugInstruction();
            //if (prev != nullptr)
            //    out << "i" << prev << " -> i" << &I << " [style=dashed];\n";
        }
        /*llvm::Instruction *psuedoLast = BB.getTerminator()->getPrevNonDebugInstruction();
        if (psuedoLast != nullptr) {
            for(auto *succ: llvm::successors(&BB)) {
                llvm::Instruction *head = &(*(succ->begin()));
                out << "i" << psuedoLast << " -> i" << head << " [style=dashed];\n";
            }
        }*/
    }
    for (auto &P: fi->prunedCtrl) {
        for (auto *dst: *P.second) {
            out << "i" << P.first << " -> i" << dst << " [style=dashed];\n";
        }
    }
    out << "}\n";
}

bool checkCallsite(llvm::Function *F, llvm::CallBase *CI) {

    std::set<llvm::Value*> args;
    for (auto arg = F->arg_begin(), end=F->arg_end(); arg!=end; ++arg) {
        args.insert(arg);
    }

    llvm::Value *sizeArg = CI->getArgOperand(0);
    llvm::APInt sizeVal;
    std::map<llvm::Value*,llvm::APInt> knowns;
    std::set<llvm::Value*> unknowns;
    if (evaluate(sizeArg, knowns, sizeVal, unknowns)) {
        llvm::outs() << "Callsite arg evaluated as " << sizeVal << "\n";
        return true;
    } else if (std::includes(args.begin(), args.end(), unknowns.begin(), unknowns.end())) {
        llvm::outs() << "Callsite arg " << *sizeArg << " depends only on args" << ": ";
        for (auto it: unknowns)
            llvm::outs() << *it << " ";
        llvm::outs() << "\n";
        return true;
    } else {
        llvm::outs() << "Callsite arg " << *sizeArg << " is dynamic" << ": ";
        for (auto it: unknowns)
            llvm::outs() << *it << " ";
        llvm::outs() << " > ";
        for (auto it: args)
            llvm::outs() << *it << " ";
        llvm::outs() << "\n";
        return false;
    }
}


void freshRegions(llvm::Module &WP, char *fname) {
    llvm::Function *malloc = findFunction(WP, fname);
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
                // If the result is true then we can determine the region size if we can substitute
                // the arguments for constants. Propagate through the call-chain checking for constant
                // args up to a threshold.
                checkCallsite(caller, CI);
                llvm::ConstantInt *sizeConst = llvm::dyn_cast<llvm::ConstantInt>(sizeArg);
                auto fi = analyseFunction(caller);
                functionFlowGraph(caller, fi.get()); //, rv);
                auto Arg = CI->getArgOperand(0);
                if (constRelativeToCall(Arg))
                    llvm::outs() << "Size is static function of input\n";
                else {
                    llvm::outs() << "Skipping call on dynamic size\n";
                    continue;
                }
                // 1. Check that the region escapes
                // 2. Use CloneFunctionInto with an appropriate VMap to make a specialised copy that passes
                // the constant argument from the call-site.
                // 3. Run an optimizer pass over the cloned function and check if the malloc arg has become
                // a contant.
                // 4. If so - build the region map for the escaped value.
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
    freshRegions(WP, argv[1]);
    llvm::llvm_shutdown();
}
