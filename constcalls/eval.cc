#include "eval.h"

llvm::APInt evaluate(llvm::Value *v, std::map<llvm::Value*,llvm::APInt> &known) {
    if (llvm::dyn_cast<llvm::Constant>(v) != nullptr) {
        llvm::outs() << "Const (true) <- " << *v << "\n";
        return true;
    }
}
