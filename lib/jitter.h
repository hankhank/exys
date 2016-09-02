#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <set>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "exys.h"

namespace Exys
{

class JitPoint;

class Jitter : public IEngine
{
public:
    Jitter(std::unique_ptr<Graph> graph);

    virtual ~Jitter();

    void PointChanged(Point& point) override;
    void Stabilize() override;
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

    static std::unique_ptr<IEngine> Build(const std::string& text);

private:
    void CompleteBuild();
    void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes);

    // LLVM helpers
    llvm::Value* GetPtrForPoint(Point& point);
    llvm::Value* JitNode(llvm::IRBuilder<>&  builder, const JitPoint& jp);

    // def memleaks here but llvm doesnt doc how to pull down
    // the exec engine and the examples I've hit segfaults
    llvm::LLVMContext* mLlvmContext;
    llvm::ExecutionEngine* mLlvmExecEngine;
    llvm::Function* mStabilizeFunc = nullptr;
    void (*mRawStabilizeFunc)() = nullptr;
    
    std::vector<Point> mInputPoints;
    std::vector<Point> mOutputPoints;
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    uint64_t mStabilisationId=1;
    bool mDirty = true;

    std::unique_ptr<Graph> mGraph;
};

};
