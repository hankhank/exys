
#include "jitter.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>

namespace Exys
{

Jitter::Jitter(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

std::string Jitter::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
}

void Jitter::TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes)
{
    if (height > node->mHeight) node->mHeight = height;
    height++;

    necessaryNodes.insert(node);
    for(auto parent : node->mParents)
    {
        TraverseNodes(parent, height, necessaryNodes);
    }
}

using namespace llvm;

void Jitter::CompleteBuild()
{
#if 0
    // Collect necessary nodes - nodes that are inputs to an observable
    // node. Also set the heights from observability
    std::set<Node::Ptr> necessaryNodes;
    std::unordered_map<Node::Ptr, std::string> observers;
    for(auto ob : mGraph->GetObservers())
    {
        uint64_t height=0;
        TraverseNodes(ob.second, height, necessaryNodes);
        observers[ob.second] = ob.first;
    }

    // Second pass - set heights and add parents/children and collect inputs and observers
    for(auto node : necessaryNodes)
    {
        if(node->mKind == Node::KIND_CONST)
        {
            //point = std::stod(node->mToken);
        }
        else if(node->mKind == Node::KIND_INPUT)
        {
            //mInputs[node->mToken] = &point;
        }
        std::unordered_map<Node::Ptr, std::string>::iterator ob;
        if((ob = observers.find(node)) != observers.end())
        {
            //mObservers[ob->second] = &point;
        }
    }
#endif

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    auto Owner = std::make_unique<llvm::Module>("exys", mLlvmContext);
    llvm::Module *M = Owner.get();
  
    std::vector<llvm::Type*> args;

    llvm::Function *stabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction("stabilizeFunc", 
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(mLlvmContext), // void return
                        args,
                        false))); // no var args

    auto *BB = llvm::BasicBlock::Create(mLlvmContext, "StabilizeBlock", stabilizeFunc);

    llvm::IRBuilder<> builder(BB);

    volatile double test[2];
    test[0] = 100.0;
    test[1] = 0;

    auto* in = llvm::ConstantInt::get(llvm::Type::getInt64Ty(mLlvmContext), (uintptr_t)&test[0]);
    auto* out = llvm::ConstantInt::get(llvm::Type::getInt64Ty(mLlvmContext), (uintptr_t)&test[1]);

    llvm::Value* inptr = llvm::ConstantExpr::getIntToPtr(in, llvm::Type::getDoublePtrTy(mLlvmContext));
    llvm::Value* outptr = llvm::ConstantExpr::getIntToPtr(out, llvm::Type::getDoublePtrTy(mLlvmContext));

    //auto* loadIn = builder.CreateLoad(inptr);

    //llvm::Value *Add = builder.CreateAdd(loadIn, loadIn);
    //llvm::Value *addtwo = builder.CreateAdd(loadIn, Add);

    //auto* storeOut = builder.CreateStore(addtwo, outptr);
    //auto* storeOut = builder.CreateStore(loadIn, outptr);

    builder.CreateRetVoid();

  // Now we create the JIT.
  //EE
  //std::string error; ExecutionEngine *ee = EngineBuilder(module).create();
    std::string error;
  llvm::ExecutionEngine* EE = llvm::EngineBuilder(std::move(Owner)).setEngineKind(llvm::EngineKind::JIT).setErrorStr(&error).create();
  std::cout << error;
  assert(EE);
  llvm::outs() << "We just constructed this LLVM module:\n\n" << *M;

  std::cout << "\n\nRunning foo: ";

  // Call the `foo' function with no arguments:
  std::vector<llvm::GenericValue> noargs;
  llvm::GenericValue gv = EE->runFunction(stabilizeFunc, noargs);
  std::cout << test[0];
  std::cout << "\n";
  std::cout << (uintptr_t)&test[0];
  std::cout << "\n";
  std::cout << test[1];
  std::cout << "\n";
  std::cout << (uintptr_t)&test[1];
  std::cout << "\n";

    Stabilize();
}

bool Jitter::IsDirty()
{
    return false;
}

void Jitter::Stabilize()
{
}

void Jitter::PointChanged(Point& point)
{
}

bool Jitter::HasInputPoint(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& Jitter::LookupInputPoint(const std::string& label)
{
    //assert(HasInputPoint(label));
    //auto niter = mInputs.find(label);
    //return niter->second->mPoint;
}

std::vector<std::string> Jitter::GetInputPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Jitter::DumpInputs()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mInputs)
    {
        //ret[ip.first] = ip.second->mPoint.mD;
    }
    return ret;
}

bool Jitter::HasObserverPoint(const std::string& label)
{
    auto niter = mObservers.find(label);
    return niter != mObservers.end();
}

Point& Jitter::LookupObserverPoint(const std::string& label)
{
    //assert(HasObserverPoint(label));
    //auto niter = mObservers.find(label);
    //return niter->second->mPoint;
}

std::vector<std::string> Jitter::GetObserverPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Jitter::DumpObservers()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mObservers)
    {
        //ret[ip.first] = ip.second->mPoint.mD;
    }
    return ret;
}

std::unique_ptr<Graph> BuildAndLoadGraph()
{
    auto graph = std::make_unique<Graph>();
    std::vector<Procedure> procedures;
    //for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

std::unique_ptr<IEngine> Jitter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Build(Parse(text));
    auto engine = std::make_unique<Jitter>(std::move(graph));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

