#ifndef _WIN32
#pragma once

#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <set>

#include "llvm/IR/IRBuilder.h"

#include "exys.h"

namespace llvm
{
    class Value;
    class Module;
    class LLVMContext;
    class ExecutionEngine;
    class Function;
};

namespace Exys
{

class JitPoint;

class Jitter : public IEngine
{
public:
    Jitter(std::unique_ptr<Graph> graph);

    virtual ~Jitter();

    void Stabilize(bool force=false) override;
    bool IsDirty() override;

    bool HasInputPoint(const std::string& label) override;
    Point& LookupInputPoint(const std::string& label) override;
    std::vector<std::string> GetInputPointLabels() override;
    std::unordered_map<std::string, double> DumpInputs() override;

    bool HasObserverPoint(const std::string& label) override;
    Point& LookupObserverPoint(const std::string& label) override;
    std::vector<std::string> GetObserverPointLabels() override;
    std::unordered_map<std::string, double> DumpObservers() override;

    std::string GetDOTGraph() override;
    std::string GetLlvmIR() const;
    std::string GetLlvmIRClone() const;
    std::string GetMemoryLayout() const;

    static std::unique_ptr<IEngine> Build(const std::string& text);

private:
    void CompleteBuild();
    void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes);

    // LLVM helpers
    llvm::Value* GetPtrForPoint(Point& point);
    llvm::Value* JitNode(llvm::Module* M, llvm::IRBuilder<>&  builder, const JitPoint& jp);

    // def memleaks here but llvm doesnt doc how to pull down
    // the exec engine and the examples I've hit segfaults
    llvm::LLVMContext* mLlvmContext = nullptr;
    llvm::ExecutionEngine* mLlvmExecEngine = nullptr;
    llvm::ExecutionEngine* mLlvmExecEngineClone = nullptr;
    llvm::Function* mStabilizeFunc = nullptr;
    llvm::Function* mStabilizeFuncClone = nullptr;
    void (*mRawStabilizeFunc)() = nullptr;
    void (*mRawStabilizeFuncClone)() = nullptr;
    
    std::vector<Point> mPoints;
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::unique_ptr<Graph> mGraph;
};

};
#endif
