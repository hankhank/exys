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

namespace Exys
{

typedef void (*StabilizationFunc)(double* inputs, double* observers);
typedef void (*CaptureState)();
typedef void (*ResetState)();

class JitPoint;

struct JitPointDescription
{
    std::string label;
    int offset;
};

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

    static std::unique_ptr<IEngine> Build(const std::string& text);

private:
    std::unique_ptr<llvm::Module> BuildModule();
    StabilizationFunc BuildJitEngine(std::unique_ptr<llvm::Module> module);
    void CompleteBuild();

    // LLVM helpers
    llvm::Value* GetPtrForPoint(Point& point);
    llvm::Value* JitNode(llvm::Module* M, llvm::IRBuilder<>&  builder, 
        const JitPoint& jp, llvm::Value* inputs, llvm::Value* observers);

    // def memleaks here but llvm doesnt doc how to pull down
    // the exec engine and the examples I've hit segfaults
    llvm::LLVMContext* mLlvmContext = nullptr;
    StabilizationFunc mRawStabilizeFunc = nullptr;
    
    std::vector<Point> mPoints;
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::unique_ptr<Graph> mGraph;
};

};
#endif
