#ifndef _WIN32

#include <iostream>
#include <sstream>
#include <cassert>

#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"

#include "gputer.h"

namespace Exys
{

Gputer::Gputer(std::unique_ptr<Jitter> jitter)
:mJitter(std::move(jitter))
{
}

Gputer::~Gputer()
{
}

std::string Gputer::GetDOTGraph()
{
    return mJitter->GetDOTGraph();
}

void Gputer::CompleteBuild()
{
    LLVMInitializeNVPTXTarget();
    LLVMInitializeNVPTXAsmPrinter();
    LLVMInitializeNVPTXTargetInfo();
    LLVMInitializeNVPTXTargetMC();

    auto module = mJitter->BuildModule();
    auto* M = module.get();

    std::string Err;
    const llvm::Target *ptxTarget = llvm::TargetRegistry::lookupTarget("nvptx", Err);
    if (!ptxTarget) 
    {
        std::cout << Err;
        assert(ptxTarget);
      // no nvptx target is available
    }

    if(!ptxTarget->hasTargetMachine())
    {
      // no backend for ptx gen available
    }
    
    std::string PTXTriple("nvptx64-nvidia-cuda");
    std::string PTXCPU = "sm_35";
    llvm::TargetOptions PTXTargetOptions = llvm::TargetOptions();
    auto* ptxTargetMachine = ptxTarget->createTargetMachine(PTXTriple, PTXCPU, "", PTXTargetOptions);

    // Generate PTX assembly
    std::string buffer;
    llvm::raw_string_ostream OS(buffer);
    {
        // TODO: put these in the constructor
        llvm::legacy::PassManager *PM2 = new llvm::legacy::PassManager();
        llvm::buffer_ostream FOS(OS);
        ptxTargetMachine->addPassesToEmitFile(*PM2, FOS, llvm::TargetMachine::CGFT_AssemblyFile);
        PM2->run(*M);
        delete PM2;
    }
    std::cout << OS.str();
                         
    //mLlvmExecEngine = llvm::EngineBuilder(std::move(Owner))
    //                            .setEngineKind(llvm::EngineKind::JIT)
    //                            .setOptLevel(llvm::CodeGenOpt::Level::Aggressive)
    //                            .setErrorStr(&error)
    //                            .create(ptxTargetMachine);
    //if(!mLlvmExecEngine)
    //{
    //    // throw here
    //    std::cout << error;
    //    assert(mLlvmExecEngine);
    //}
    //mRawStabilizeFunc = (void(*)()) mLlvmExecEngine->getPointerToFunction(mStabilizeFunc);
    //mLlvmExecEngine->finalizeObject();

    //Stabilize();
}

std::unique_ptr<Gputer> Gputer::Build(const std::string& text)
{
    auto jitter = Jitter::Build(text);
    auto engine = std::make_unique<Gputer>(std::move(jitter));
    engine->CompleteBuild();
    return engine;
}

}

#endif
