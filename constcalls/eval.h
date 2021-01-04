#pragma once

#include <set>
#include <map>
#include "llvm/ADT/APInt.h"

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

bool evaluate(llvm::Value *v, std::map<llvm::Value*,llvm::APInt> &known, llvm::APInt &result,
              std::set<llvm::Value*> &unknowns);
