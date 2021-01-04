#include "eval.h"

/* The evaluator computes the result of expression relative to a set of known constants.
   This is a form of partial evaluation that will be applied to calls when we know some
   of the argument values.

   There are two approaches that can be used:
   1. Track the dictionary of known constants as a state passed through an evaluator.
   2. Clone the function, rewrite the arguments as contants and apply constant folding
      (and any auxillery transform passes that enable its application).

   The second approach requires tracking a correspondence through the transformations,
   i.e. v1=v1', so that values in the optimized output can be mapped back onto the
   values in the original to annotate the results.

   Trying approach (1) first as I cannot think of a good way to track the correspondence.

   On failure the set of unknowns is expanded to include values which were not constant
   during the attempted evaluation.
*/

bool evaluate(llvm::Value *v, std::map<llvm::Value*,llvm::APInt> &known, llvm::APInt &result,
              std::set<llvm::Value*> &unknowns) {
    auto *ci = llvm::dyn_cast<llvm::ConstantInt>(v);
    if (ci!=nullptr) {
        result = ci->getValue();
        return true;
    }
    auto it = known.find(v);
    if (it!=known.end() ) {
        result = it->second;
        return true;
    }
    llvm::Instruction *i = llvm::dyn_cast<llvm::Instruction>(v);
    llvm::APInt left,right;
    if (i!=nullptr && i->getNumOperands()==1) {
        if (!evaluate(i->getOperand(0), known, left, unknowns))
            return false;
        switch (i->getOpcode()) {
        case llvm::Instruction::SExt:
            llvm::outs() << "Sign extend from " << i->getOperand(0)->getType() << " to " << i->getType() << "\n";
            break;
        }
        llvm::outs() << "Evaluator has no case for " << *v << "\n";
    }
    if (i!=nullptr && i->getNumOperands()==2) {
        // Avoid shortcut logic to fill unknowns
        bool e1 = evaluate(i->getOperand(0), known, left, unknowns),
             e2 = evaluate(i->getOperand(1), known, right, unknowns);
        if (!e1 || !e2)
            return false;
        switch (i->getOpcode()) {
        case llvm::Instruction::Add:
            result = left + right;
            return true;
        }
        llvm::outs() << "Evaluator has no case for " << i->getOpcodeName() << "\n";
    }
    unknowns.insert(v);
    llvm::outs() << "Evaluator failed on " << *v << "\n";
    return false;
}
