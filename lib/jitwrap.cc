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

std::string JitWrap::GetDOTGraph()
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
    mCaptureFunc = reinterpret_cast<CaptureFunc>(llvmExecEngine->getPointerToNamedFunction(CAP_FUNC_NAME));
    mResetFunc = reinterpret_cast<ResetFunc>(llvmExecEngine->getPointerToNamedFunction(RESET_FUNC_NAME));

    llvmExecEngine->finalizeObject();
    
    // Setup memory
    const auto& inputDesc = mJitter->GetInputDesc();
    const auto& observerDesc = mJitter->GetObserverDesc();
    mPoints.resize(inputDesc.size() + observerDesc.size());

    for(const auto& id : inputDesc)
    {
        for(const auto& label : id.mLabels) mInputs[label] = &mPoints[id.mOffset];
    }
    for(const auto& od : observerDesc)
    {
        for(const auto& label : od.mLabels) mObservers[label] = &mPoints[od.mOffset];
    }
    mInputPtr = &mPoints.front();
    mObserverPtr = &mPoints.front() + inputDesc.size();
}

void JitWrap::CompleteBuild()
{
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    
    BuildJitEngine(mJitter->BuildModule());

    Stabilize(true);
}

bool JitWrap::IsDirty()
{
    for(const auto& namep : mInputs)
    {
        auto& point = *namep.second;
        if(point.IsDirty()) return true;
    }
    return false;
}

void JitWrap::Stabilize(bool force)
{
    if(force || IsDirty())
    {
        mRawStabilizeFunc(mInputPtr, mObserverPtr);
        for(auto& p : mPoints) p.Clean();
    }
}

bool JitWrap::HasInputPoint(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& JitWrap::LookupInputPoint(const std::string& label)
{
    assert(HasInputPoint(label));
    auto niter = mInputs.find(label);
    return *niter->second;
}

std::vector<std::string> JitWrap::GetInputPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> JitWrap::DumpInputs()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mInputs)
    {
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

bool JitWrap::HasObserverPoint(const std::string& label)
{
    auto niter = mObservers.find(label);
    return niter != mObservers.end();
}

Point& JitWrap::LookupObserverPoint(const std::string& label)
{
    assert(HasObserverPoint(label));
    auto niter = mObservers.find(label);
    return *niter->second;
}

std::vector<std::string> JitWrap::GetObserverPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> JitWrap::DumpObservers()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mObservers)
    {
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

std::unique_ptr<IEngine> JitWrap::Build(const std::string& text)
{
    auto jitter = Jitter::Build(text);
    auto engine = std::make_unique<JitWrap>(std::move(jitter));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

#endif
