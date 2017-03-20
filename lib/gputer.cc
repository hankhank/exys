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
    std::string PTXTriple("nvptx64-unknown-cuda");
    std::string PTXCPU = "sm_35";

    const llvm::Target *ptxTarget = llvm::TargetRegistry::lookupTarget(PTXTriple, Err);
    if (!ptxTarget) 
    {
        std::cout << Err;
        assert(false && "no nvptx target is available");
    }

    if(!ptxTarget->hasTargetMachine())
    {
        assert(false && "no backend for ptx gen available");
    }
    
    llvm::TargetOptions PTXTargetOptions = llvm::TargetOptions();
    auto* ptxTargetMachine = ptxTarget->createTargetMachine(PTXTriple, PTXCPU, "", PTXTargetOptions,
             llvm::Reloc::PIC_, llvm::CodeModel::Default, llvm::CodeGenOpt::Aggressive);

    // 64-bit
    M->setDataLayout(llvm::StringRef("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-"
                "f64:64:64-v16:16:16-v32:32:32-v64:64:64-v128:128:128-n16:32:64"));

    // Generate PTX assembly
    llvm::raw_string_ostream OS(mPtxBuf);
    {
        // TODO: put these in the constructor
        llvm::legacy::PassManager *PM2 = new llvm::legacy::PassManager();
        llvm::buffer_ostream FOS(OS);
        ptxTargetMachine->addPassesToEmitFile(*PM2, FOS, llvm::TargetMachine::CGFT_AssemblyFile);
        PM2->run(*M);
        delete PM2;
    }
}

std::unique_ptr<Gputer> Gputer::Build(const std::string& text)
{
    auto jitter = Jitter::Build(text);
    auto engine = std::unique_ptr<Gputer>(new Gputer(std::move(jitter)));
    engine->CompleteBuild();
    return engine;
}

}

#endif
