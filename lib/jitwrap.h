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
    JitWrap(JitWrap& jw);

    virtual ~JitWrap();

    void Stabilize(bool force=false) override;
    bool IsDirty() const override;

    bool HasInputPoint(const std::string& label) const override;
    Point& LookupInputPoint(const std::string& label) override;
    std::vector<std::string> GetInputPointLabels() const override;
    std::vector<std::pair<std::string, double>> DumpInputs() const override;

    bool HasObserverPoint(const std::string& label) const override;
    Point& LookupObserverPoint(const std::string& label) override;
    std::vector<std::string> GetObserverPointLabels() const override;
    std::vector<std::pair<std::string, double>> DumpObservers() const override;

    bool SupportSimulation() const override;
    int GetNumSimulationFunctions() const override;
    void CaptureState() override;
    void ResetState() override;
    bool RunSimulationId(int simId) override;
    std::string GetNumSimulationTarget(int simId) const override;

    std::string GetDOTGraph() const override;

    void CopyState(JitWrap& jw);

    static std::unique_ptr<IEngine> Build(const std::string& text);

private:
    void BuildJitEngine(std::unique_ptr<llvm::Module> module);
    void SetPointPtrs();
    void CompleteBuild();

    InitFunc mInitFunc = nullptr;
    StabilizationFunc mRawStabilizeFunc = nullptr;
    SimFunc mRawSimFunc = nullptr;

    int mSimFuncCount = 0;
    std::vector<std::string> mSimFuncTargets;
    
    std::vector<double> mState;
    std::vector<double> mStateCapture;
    std::vector<Point> mPoints;
    std::vector<Point> mPointsCapture;
    Point* mInputPtr = nullptr;
    Point* mObserverPtr = nullptr;
    int mInputSize = 0;
    std::unordered_map<std::string, int> mObserverOffsets;
    std::unordered_map<std::string, int> mInputOffsets;

    std::unique_ptr<Jitter> mJitter;
};


};
#endif
