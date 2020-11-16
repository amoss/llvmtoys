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

/*
    Syntax: a-zA-Z0-9_  literals
            .           any literal
            []          character class
            |           choice (either the left or right regex)
            *
            ^           anchor path to call-graph entry
            >           call
            >+          1+ calls

    Examples:
            .*                     any (reachable) function in the program
            .*>.*                  any caller/callee pair
            .*>+.*                 any path through the call-graph
            .*>+.*thread           any path that ends in a function whose name ends in "thread"
            main>uv_.*             any call from main to a function whose name starts in "uv_"
            main>+rrd.*>.*disk     any path from main to a function whose name ends in "disk",
                                    that passes through a function whose name starts with "rrd"

    Notes:
            There are explicit anchors for paths, and implict anchors for function names.
            i.e. each function-name level regex has an implict ^..., and if the path anchor
            is not used it implies ^>+ as a prefix.
            around the regex. To match within the name instead use .* as a prefix/suffix.
*/

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


void emitResult(std::vector<std::string> &path) {
    for(auto P: path)
        llvm::outs() << P << " ";
}


int matchNodeName(const char *regex, const char *name) {
    const char *start = regex;
    //std::cout << "Test: " << regex << " -> " << name << std::endl;
    while (*regex!=0) {
        if (*regex=='>')
        {
            if (*name==0)
                return regex-start;
            return -1;
        }

        if ( (*regex>='a' && *regex<='z') || (*regex>='A' && *regex<='Z') || 
             (*regex>='0' && *regex<='9') || *regex=='_' || *regex=='.') {
            if (regex[1]=='*') {
                while( (*regex=='.' && *name!=0) || *regex==*name)
                {
                    int r = matchNodeName(regex+2,name);
                    if (r>=0)
                        return r+2;
                    name++;
                }
                regex+=2;
                continue;
            }
            if (*regex == '.' || *name==*regex) {
                regex++;
                name++;
                continue;
            }
            return -1;
        }

        /*else if (*regex=='|')
            ...

        else if (*regex=='[')
            ...*/

        else
            throw std::string("Invalid regex:") + regex;

    }
    if (*name==0)
        return regex-start;
    return -1;
}


void matchSuccs(llvm::CallGraphNode* node, std::vector<std::string> &path, std::set<llvm::CallGraphNode*> &visited,
           bool orMany, char *regex);

void match(llvm::CallGraphNode* node, std::vector<std::string> &path, std::set<llvm::CallGraphNode*> &visited, 
           bool orMany, char *regex) {
    auto *F = node->getFunction();
    if (!F || !F->hasName())        // null nodes gets inserted as calls from any external node
        return;
    std::string name = F->getName().str();
    visited.insert(node);
    if (*regex==0) {
        emitResult(path);
        llvm::outs() << "\n";
        return;                 // continue matching >+ as suffix?
    }
    path.push_back(name);

    if (*regex=='>') {
        char *nxtRegex;
        bool nxtOrMany;
        if (regex[1]=='+')
            matchSuccs(node, path, visited, true,  regex+2);
        else
            matchSuccs(node, path, visited, false, regex+1);
        path.pop_back();
        return;
    }

    int m = matchNodeName(regex,name.c_str());
    if (m<0 && !orMany) {
        path.pop_back();
        return;
    }

    if (m>0) {
        if (regex[m]==0) {
            emitResult(path);
            llvm::outs() << "\n";
        }
        else
            matchSuccs(node, path, visited, false, regex+m);     // path-step, overwrite orMany flag
    }

    if (orMany)
        matchSuccs(node, path, visited, true, regex);
    path.pop_back();
}

void matchSuccs(llvm::CallGraphNode* node, std::vector<std::string> &path, std::set<llvm::CallGraphNode*> &visited,
           bool orMany, char *regex) {
    for (auto succ: *node) {
        if (visited.find(succ.second) == visited.end()) {
            match(succ.second, path, visited, orMany, regex);
        }
    }
}

void regexCG(llvm::CallGraph &cg, char *regex) {
    std::set<llvm::CallGraphNode*> visited;
    auto *outside = cg.getExternalCallingNode();
    std::vector<std::string> path;
    int m, n = strlen(regex);
    for (auto entry: *outside) {
        auto *F = entry.second->getFunction();
        if (!F) {
            std::cout << "Skipping entry with no function? " << entry.second << std::endl;
            continue;
        }
        path.clear();
        visited.clear();
        if (*regex=='^')
            match(entry.second, path, visited, false, regex+1);
        else
            match(entry.second, path, visited, true,  regex);
    }

    return;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "usage: callgrep <path-regex> <IR files>...\n";
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

    llvm::CallGraph cg(WP);
    //cg.print(llvm::outs());
    try {
      regexCG(cg, argv[1]);
    }
    catch(std::string &err) {
      std::cout << "Error: " << err << std::endl;
    }
    llvm::llvm_shutdown();
}

