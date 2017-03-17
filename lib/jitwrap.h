#ifndef _WIN32
#pragma once

#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

#include "exys.h"
#include "jitter.h"

namespace Exys
{

class JitWrap : public IEngine
{
public:
    JitWrap(std::unique_ptr<Jitter> graph);

    virtual ~JitWrap();

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
    void BuildJitEngine(std::unique_ptr<llvm::Module> module);
    void CompleteBuild();

    StabilizationFunc mRawStabilizeFunc = nullptr;
    
    std::vector<double> mState;
    std::vector<Point> mPoints;
    Point* mInputPtr = nullptr;
    Point* mObserverPtr = nullptr;
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::unique_ptr<Jitter> mJitter;
};

};
#endif
