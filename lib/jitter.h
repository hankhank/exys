#ifndef _WIN32
#pragma once

#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <set>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

#include "exys.h"

namespace llvm
{
    class Value;
    class LLVMContext;
    class ExecutionEngine;
    class Function;
};

namespace 
{
    const std::string STAB_FUNC_NAME  = "ExysStabilize";
    const std::string CAP_FUNC_NAME   = "ExysCaptureState";
    const std::string RESET_FUNC_NAME = "ExysResetState";
    const std::string POINT_NAME      = "Point";
};

namespace Exys
{

typedef void (*StabilizationFunc)(Point* inputs, Point* observers);
typedef void (*CaptureFunc)();
typedef void (*ResetFunc)();

class JitPoint;

struct JitPointDescription
{
    std::vector<std::string> mLabels;
    uint64_t mOffset = 0;
    uint32_t mLength = 0;
};

class Jitter
{
public:
    Jitter(std::unique_ptr<Graph> graph);
    virtual ~Jitter();

    static std::unique_ptr<Jitter> Build(const std::string& text);

    const std::vector<Node::Ptr>& GetInputDesc() const { return mInputs; }
    const std::vector<Node::Ptr>& GetObserverDesc() const { return mObservers; }

    std::unique_ptr<llvm::Module> BuildModule();

    std::string GetDOTGraph();
private:
    // LLVM helpers
    llvm::Value* JitNode(llvm::Module* M, llvm::IRBuilder<>&  builder, 
        const JitPoint& jp, llvm::Value* inputs, llvm::Value* observers);

    // def memleaks here but llvm doesnt doc how to pull down
    // the exec engine and the examples I've hit segfaults
    llvm::LLVMContext* mLlvmContext = nullptr;
    
    std::vector<Node::Ptr> mInputs;
    std::vector<Node::Ptr> mObservers;

    std::unique_ptr<Graph> mGraph;
};

};
#endif
