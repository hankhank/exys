
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

ComputeFunction Interpreter::LookupComputeFunction(Node::Ptr node)
{
    if(node->mKind == Node::KIND_CONST || node->mKind == Node::KIND_INPUT)
    {
        return ConstDummy;
    }
    for(auto& proc : AVAILABLE_PROCS)
    {
        if(node->mToken.compare(proc.procedure.id) == 0)
        {
            return proc.func;
        }
    }
    assert(false);
    return nullptr;
}

llvm::Value* Jitter::GetPtrForPoint(Point& point)
{
    auto* ptrAsInt = llvm::ConstantInt::get(llvm::Type::getInt64Ty(mLlvmContext), (uintptr_t)&point.mD);
    return llvm::ConstantExpr::getIntToPtr(ptrAsInt, llvm::Type::getDoublePtrTy(mLlvmContext));
}

struct JitPoint
{
    llvm::Value* mValue = nullptr;
    Point* mPoint = nullptr;
    bool mIsObserver = false;
    uint64_t mHeight;

    std::vector<JitPoint*> mParents;
    std::vector<JitPoint*> mChildren;

    bool operator!=(const InterPoint& rhs) { return mPoint != rhs.mPoint; }

    InterPoint& operator=(InterPoint ip) {mPoint = ip.mPoint; return *this;}
    InterPoint& operator=(double d)      {mPoint = d; return *this;}
};

void Jitter::CompleteBuild()
{
    // Collect necessary nodes - nodes that are inputs to an observable
    // node. Also set the heights from observability
    std::set<Node::Ptr> necessaryNodes;
    for(auto ob : mGraph->GetObservers())
    {
        uint64_t height=0;
        TraverseNodes(ob.second, height, necessaryNodes);
    }

    // Second pass - set heights and add parents/children and collect inputs and observers
    std::set<JitPoint> mRecomputeHeap;
    std::unordered_map<Node::Ptr, std::string> inputs;
    for(auto node : necessaryNodes)
    {
        for(auto pnode : node->mParents)
        {
            auto& parent = mInterPointGraph[FindNodeOffset(necessaryNodes, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }
    }
    
    std::set<HeightPtrPair> mRecomputeHeap;
    for(auto& hpp : mRecomputeHeap)
    {
        hpp.value = GenerateFunction(hpp);
        if(hpp.observer)
        {
            builder.CreateStore(hpp.value, GetPtrForPoint(hpp.point));
        }
    }

    mInterPointGraph.resize(necessaryNodes.size());

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    auto Owner = std::make_unique<llvm::Module>("exys", mLlvmContext);
    llvm::Module *M = Owner.get();
  
    std::vector<llvm::Type*> args;

    llvm::Function *stabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction("StabilizeFunc", 
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(mLlvmContext), // void return
                        args,
                        false))); // no var args

    auto *BB = llvm::BasicBlock::Create(mLlvmContext, "StabilizeBlock", stabilizeFunc);

    llvm::IRBuilder<> builder(BB);

    auto* loadIn = builder.CreateLoad(inptr);

    llvm::Value *Add = builder.CreateFAdd(loadIn, loadIn);
    llvm::Value *addtwo = builder.CreateFAdd(loadIn, Add);

    auto* storeOut = builder.CreateStore(addtwo, outptr);
    //auto* storeOut = builder.CreateStore(Add, outptr);

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
  EE->finalizeObject();
  llvm::GenericValue gv = EE->runFunction(stabilizeFunc, noargs);

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

