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
    class Block;
};

namespace 
{
    const std::string STAB_FUNC_NAME = "ExysStabilize";
    const std::string SIM_FUNC_NAME  = "ExysSim";
    const std::string POINT_NAME     = "Point";
};

namespace Exys
{

typedef void (*StabilizationFunc)(Point* inputs, Point* observers, double* state);
typedef void (*SimFunc)(Point* inputs, Point* inputsAndDone, double* state, int simId);

class JitPoint;

struct JitPointDescription
{
    std::vector<std::string> mLabels;
    uint64_t mOffset = 0;
    uint32_t mLength = 0;
};

typedef std::function<llvm::Value* (llvm::Module*, llvm::IRBuilder<>&, const JitPoint&)> JitComputeFunction;

struct JitPointProcessor
{
    Procedure procedure;
    JitComputeFunction func; 
};

class Jitter
{
public:
    Jitter();
    virtual ~Jitter();

    static std::unique_ptr<Jitter> Build(const std::string& text);
    
    const std::vector<Node::Ptr>& GetInputDesc() const { return mInputs; }
    const std::vector<Node::Ptr>& GetObserverDesc() const { return mObservers; }
    int GetStateSpaceSize() const { return mNumStatePtr; }
    int GetSimFuncCount() const { return mNumSimFunc; }

    std::unique_ptr<llvm::Module> BuildModule();

    std::string GetDOTGraph();
private:
    void AssignGraph(std::unique_ptr<Graph>& graph);
    std::unique_ptr<Graph> BuildAndLoadGraph();

    // def memleaks here but llvm doesnt doc how to pull down
    // the exec engine and the examples I've hit segfaults
    llvm::LLVMContext* mLlvmContext = nullptr;
    llvm::Value* mStatePtr = nullptr;
    int mNumStatePtr = 0;
    int mNumSimFunc = 0;
    
    std::vector<Node::Ptr> mInputs;
    std::vector<Node::Ptr> mObservers;

    std::unique_ptr<Graph> mGraph;
    std::vector<JitPointProcessor> mPointProcessors;

    // LLVM helpers
    llvm::BasicBlock* BuildBlock(const std::string& blockName, const std::vector<Node::Ptr>& nodeLayout, 
            llvm::Function* func, llvm::Module *M, llvm::Value* inputsPtr, llvm::Value* observersPtr, llvm::Value* statePtr,
            bool ret=true);
    llvm::Value* JitGV(llvm::Module* M, llvm::IRBuilder<>& builder);
    llvm::Value* JitNode(llvm::Module* M, llvm::IRBuilder<>&  builder, 
        const JitPoint& jp, llvm::Value* inputs, llvm::Value* observers);
    llvm::Value* JitLatch(llvm::Module*, llvm::IRBuilder<>&, const JitPoint&);
    llvm::Value* JitFlipFlop(llvm::Module*, llvm::IRBuilder<>&, const JitPoint&);
    llvm::Value* JitStore(llvm::Module*, llvm::IRBuilder<>&, const JitPoint&);
    llvm::Value* JitLoad(llvm::Module*, llvm::IRBuilder<>&, const JitPoint&);
    llvm::Value* JitTick(llvm::Module*, llvm::IRBuilder<>&, const JitPoint&);
};

};
#endif
