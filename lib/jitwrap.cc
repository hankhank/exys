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
:mJitter(std::move(jitter))
{
}

JitWrap::~JitWrap()
{
}

std::string JitWrap::GetDOTGraph() const
{
    return mJitter->GetDOTGraph();
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
    mPoints.resize(inputDesc.size() + observerDesc.size() + 1);
    mState.resize(mJitter->GetStateSpaceSize());

    for(const auto& id : inputDesc)
    {
        for(const auto& label : id->mInputLabels)
        {
            assert(id->mOffset < inputDesc.size());
            mInputOffsets[label] = id->mOffset;
            auto& ip = mPoints[id->mOffset];
            ip.mLength = id->mLength;
        }
    }
    for(const auto& od : observerDesc)
    {
        for(const auto& label : od->mObserverLabels)
        {
            int obOffset = inputDesc.size()+1+od->mOffset;
            assert(obOffset < mPoints.size());
            mObserverOffsets[label] = obOffset;
            auto& op = mPoints[inputDesc.size()+1+od->mOffset];
            op.mLength = od->mLength;
        }
    }
    mInputPtr = &mPoints.front();
    mObserverPtr = &mPoints.front() + inputDesc.size() + 1;
    mInputSize = inputDesc.size();
    mInitFunc(mState.data());
    // we shift the observers by one so we can fit done flag
    // at the end
}

void JitWrap::CompleteBuild()
{
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    
    BuildJitEngine(mJitter->BuildModule());

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
    return mJitter->GetSimFuncCount();
}

void JitWrap::CaptureState()
{
    std::copy(mState.begin(), mState.end(),
        std::back_inserter(mStateCapture));
    std::copy(mPoints.begin(), mPoints.end(),
        std::back_inserter(mPointsCapture));
}

void JitWrap::ResetState()
{
    std::copy(mStateCapture.begin(), mStateCapture.end(),
        std::back_inserter(mState));
    std::copy(mPointsCapture.begin(), mPointsCapture.end(),
        std::back_inserter(mPoints));
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
    const auto& targets = mJitter->GetSimFuncTargets();
    assert(simId >= targets.size() && "simid index out of range");
    if(simId < targets.size())
    {
        return targets[simId];
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
