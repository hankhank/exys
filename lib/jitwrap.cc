#ifndef _WIN32

#include <iostream>
#include <sstream>
#include <cassert>

#include "llvm/ExecutionEngine/MCJIT.h"

#include "jitwrap.h"
#include "helpers.h"

namespace Exys
{

JitWrap::JitWrap(std::unique_ptr<Jitter> jitter)
: mJitter(std::move(jitter))
{
}

// Use this constructor if you want run the simulations
// against a second memory location i.e. in a thread
JitWrap::JitWrap(JitWrap& jw)
: mRawStabilizeFunc(jw.mRawStabilizeFunc)
, mRawSimFunc(jw.mRawSimFunc)
, mSimFuncCount(jw.mSimFuncCount)
, mSimFuncTargets(jw.mSimFuncTargets)
, mState(jw.mState)
, mPoints(jw.mPoints)
, mInputSize(jw.mInputSize)
, mObserverOffsets(jw.mObserverOffsets)
, mInputOffsets(jw.mInputOffsets)
{
    SetPointPtrs();
}

// Copy to capture so calling convention is simplified slightly
// where you can always call reset before a simulation
void JitWrap::CopyState(JitWrap& jw)
{
    std::copy(jw.mState.begin(), jw.mState.end(),
        std::back_inserter(mStateCapture));
    std::copy(jw.mPoints.begin(), jw.mPoints.end(),
        std::back_inserter(mPointsCapture));
    SetPointPtrs();
}

JitWrap::~JitWrap()
{
}

std::string JitWrap::GetDOTGraph() const
{
    if(mJitter) return mJitter->GetDOTGraph();
    return "";
}

void JitWrap::BuildJitEngine(std::unique_ptr<llvm::Module> module)
{
    std::string error;
    auto* llvmExecEngine = llvm::EngineBuilder(std::move(module))
                                .setEngineKind(llvm::EngineKind::JIT)
                                .setOptLevel(llvm::CodeGenOpt::Level::Aggressive)
                                .setErrorStr(&error)
                                .create();
    llvmExecEngine->DisableGVCompilation(true);
    if(!llvmExecEngine)
    {
        // throw here
        std::cout << error;
        assert(llvmExecEngine);
    }

    mRawStabilizeFunc = reinterpret_cast<StabilizationFunc>(llvmExecEngine->getPointerToNamedFunction(STAB_FUNC_NAME));
    mInitFunc = reinterpret_cast<InitFunc>(llvmExecEngine->getPointerToNamedFunction(INIT_FUNC_NAME));
    if(mJitter->GetSimFuncCount() > 0)
    {
        mRawSimFunc = reinterpret_cast<SimFunc>(llvmExecEngine->getPointerToNamedFunction(SIM_FUNC_NAME));
    }

    llvmExecEngine->finalizeObject();
    
    // Setup memory
    const auto& inputDesc = mJitter->GetInputDesc();
    const auto& observerDesc = mJitter->GetObserverDesc();
    mPoints.resize(inputDesc.size() + observerDesc.size());
    mState.resize(mJitter->GetStateSpaceSize());

    for(const auto& id : inputDesc)
    {
        for(const auto& label : id->mInputLabels)
        {
            assert(id->mInputOffset < inputDesc.size());
            mInputOffsets[label] = id->mInputOffset;
            auto& ip = mPoints[id->mInputOffset];
            ip.mLength = id->mLength;
        }
    }
    for(const auto& od : observerDesc)
    {
        for(const auto& label : od->mObserverLabels)
        {
            int obOffset = inputDesc.size()+od->mObserverOffset;
            assert(obOffset < mPoints.size());
            mObserverOffsets[label] = obOffset;
            auto& op = mPoints[obOffset];
            op.mLength = od->mLength;
        }
    }
    mInputSize = inputDesc.size();
    mInitFunc(mState.data());
    SetPointPtrs();
    // we shift the observers by one so we can fit done flag
    // at the end
}

void JitWrap::SetPointPtrs()
{
    mInputPtr = &mPoints.front();
    mObserverPtr = &mPoints.front() + mInputSize;
}

void JitWrap::CompleteBuild()
{
    assert(mJitter && "Can only build with underlying jitter");
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    
    BuildJitEngine(mJitter->BuildModule());

    mSimFuncCount = mJitter->GetSimFuncCount();
    mSimFuncTargets = mJitter->GetSimFuncTargets();

    Stabilize(true);
}

bool JitWrap::IsDirty() const
{
    for(const auto& namep : mInputOffsets)
    {
        const auto& point = mPoints[namep.second];
        if(point.IsDirty()) return true;
    }
    return false;
}

void JitWrap::Stabilize(bool force)
{
    if(force || mJitter->GetStateSpaceSize() || IsDirty())
    {
        mRawStabilizeFunc(mInputPtr, mObserverPtr, mState.data());
        for(auto& p : mPoints) p.Clean();
    }
}

bool JitWrap::HasInputPoint(const std::string& label) const
{
    auto niter = mInputOffsets.find(label);
    return niter != mInputOffsets.end();
}

Point& JitWrap::LookupInputPoint(const std::string& label)
{
    assert(HasInputPoint(label));
    auto niter = mInputOffsets.find(label);
    return mPoints[niter->second];
}

std::vector<std::string> JitWrap::GetInputPointLabels() const
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputOffsets)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::vector<std::pair<std::string, double>> JitWrap::DumpInputs() const
{
    std::vector<std::pair<std::string, double>> ret;
    for(const auto& ip : mInputOffsets)
    {
        ret.push_back(std::make_pair(ip.first, mPoints[ip.second].mVal));
    }
    return ret;
}

bool JitWrap::HasObserverPoint(const std::string& label) const
{
    auto niter = mObserverOffsets.find(label);
    return niter != mObserverOffsets.end();
}

Point& JitWrap::LookupObserverPoint(const std::string& label)
{
    assert(HasObserverPoint(label));
    auto niter = mObserverOffsets.find(label);
    return mPoints[niter->second];
}

std::vector<std::string> JitWrap::GetObserverPointLabels() const
{
    std::vector<std::string> ret;
    for(const auto& ip : mObserverOffsets)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::vector<std::pair<std::string, double>> JitWrap::DumpObservers() const
{
    std::vector<std::pair<std::string, double>> ret;
    for(const auto& ip : mObserverOffsets)
    {
        ret.push_back(std::make_pair(ip.first, mPoints[ip.second].mVal));
    }
    return ret;
}

bool JitWrap::SupportSimulation() const
{
    return true;
}

int JitWrap::GetNumSimulationFunctions() const
{
    return mSimFuncCount;
}

void JitWrap::CaptureState()
{
    mPointsCapture = mPoints;
    mStateCapture = mState;
}

void JitWrap::ResetState()
{
    mPoints = mPointsCapture;
    mState = mStateCapture;
}

bool JitWrap::RunSimulationId(int simId)
{
    assert(mRawSimFunc && "Simulation function does not exist");
    if(!mRawSimFunc) return true;
    mRawSimFunc(mInputPtr, mInputPtr, mState.data(), simId);
    return mInputPtr[mInputSize] != 0.0;
}

std::string JitWrap::GetNumSimulationTarget(int simId) const
{
    assert(simId >= mSimFuncTargets.size() && "simid index out of range");
    if(simId < mSimFuncTargets.size())
    {
        return mSimFuncTargets[simId];
    }

    return "";
}

std::unique_ptr<IEngine> JitWrap::Build(const std::string& text)
{
    auto jitter = Jitter::Build(text);
    auto engine = std::unique_ptr<JitWrap>(new JitWrap(std::move(jitter)));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

#endif
