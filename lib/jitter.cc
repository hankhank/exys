
#include "jitter.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>

namespace Exys
{

struct JitPoint
{
    llvm::Value* mValue = nullptr;
    Point* mPoint = nullptr;
    Node::Ptr mNode = nullptr;

    std::vector<JitPoint*> mParents;
    std::vector<JitPoint*> mChildren;

    bool operator<(const JitPoint& rhs) const
    {
        return (mNode->mHeight > rhs.mNode->mHeight);
    }
};

typedef std::function<llvm::Value* (llvm::IRBuilder<>&, const JitPoint&)> ComputeFunction;

struct JitPointProcessor
{
    Procedure procedure;
    ComputeFunction func; 
};

void DummyValidator(Node::Ptr)
{
}

llvm::Value* JitTernary(llvm::IRBuilder<>& builder, const JitPoint& point)
{
    assert(point.mParents.size() == 3);
    llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
    llvm::Value* cmp = builder.CreateFCmpOEQ(point.mParents[0]->mValue, zero);
    return builder.CreateSelect(cmp,
            point.mParents[1]->mValue,
            point.mParents[2]->mValue);
}

#define DEFINE_LOOP_OPERATOR(__FUNCNAME, __MEMFUNC) \
llvm::Value* __FUNCNAME(llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    assert(point.mParents.size() >= 2); \
    auto p = point.mParents.begin(); \
    llvm::Value *val = (*p)->mValue; \
    for(p++; p != point.mParents.end(); p++) \
    { \
        assert(val); \
        assert((*p)->mValue); \
        val = (builder.__MEMFUNC)(val, (*p)->mValue); \
    } \
    return val; \
}

DEFINE_LOOP_OPERATOR(JitDoubleAdd, CreateFAdd);
DEFINE_LOOP_OPERATOR(JitDoubleSub, CreateFSub);
DEFINE_LOOP_OPERATOR(JitDoubleMul, CreateFMul);
DEFINE_LOOP_OPERATOR(JitDoubleDiv, CreateFDiv);

JitPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",    DummyValidator},  JitTernary},
    {{"+",    DummyValidator},  JitDoubleAdd},
    {{"-",    DummyValidator},  JitDoubleSub},
    {{"/",    DummyValidator},  JitDoubleDiv},
    {{"*",    DummyValidator},  JitDoubleMul},
    ////{{"%",    DummyValidator},  LoopOperator<std::modulus<double>>},
    //{{"<",    DummyValidator},  PairOperator<std::less<double>>},
    //{{"<=",   DummyValidator},  PairOperator<std::less_equal<double>>},
    //{{">",    DummyValidator},  PairOperator<std::greater<double>>},
    //{{">=",   DummyValidator},  PairOperator<std::greater_equal<double>>},
    //{{"==",   DummyValidator},  PairOperator<std::equal_to<double>>},
    //{{"!=",   DummyValidator},  PairOperator<std::not_equal_to<double>>},
    //{{"&&",   DummyValidator},  PairOperator<std::logical_and<double>>},
    //{{"||",   DummyValidator},  PairOperator<std::logical_or<double>>},
    //{{"min",  DummyValidator},  LoopOperator<MinFunc>},
    //{{"max",  DummyValidator},  LoopOperator<MaxFunc>},
    //{{"exp",  DummyValidator},  UnaryOperator<ExpFunc>},
    //{{"ln",   DummyValidator},  UnaryOperator<LogFunc>},
    //{{"not",  DummyValidator},  UnaryOperator<std::logical_not<double>>}
};

Jitter::Jitter(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

Jitter::~Jitter()
{
    if(mLlvmExecEngine)
    {
        //delete mLlvmExecEngine;
        llvm::llvm_shutdown();
    }
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

llvm::Value* Jitter::GetPtrForPoint(Point& point)
{
    auto* ptrAsInt = llvm::ConstantInt::get(llvm::Type::getInt64Ty(mLlvmContext), (uintptr_t)&point.mD);
    return llvm::ConstantExpr::getIntToPtr(ptrAsInt, llvm::Type::getDoublePtrTy(mLlvmContext));
}

llvm::Value* Jitter::JitNode(llvm::IRBuilder<>& builder, const JitPoint& jp)
{
    llvm::Value* ret = nullptr;
    if(jp.mNode->mKind == Node::KIND_CONST)
    {
      ret = llvm::ConstantFP::get(builder.getDoubleTy(), std::stod(jp.mNode->mToken));
    }
    else if(jp.mNode->mKind == Node::KIND_INPUT)
    {
        assert(jp.mPoint);
        ret = builder.CreateLoad(GetPtrForPoint(*jp.mPoint));
    }
    else if(jp.mNode->mKind == Node::KIND_PROC)
    {
        for(auto& proc : AVAILABLE_PROCS)
        {
            if(jp.mNode->mToken.compare(proc.procedure.id) == 0)
            {
                ret = proc.func(builder, jp);
            }
        }
        assert(ret);
    }

    if(jp.mNode->mIsObserver)
    {
        assert(ret);
        assert(jp.mPoint);
        builder.CreateStore(ret, GetPtrForPoint(*jp.mPoint));
    }
    return ret;
}

size_t FindNodeOffset(const std::set<Node::Ptr>& nodes, Node::Ptr node)
{
    return std::distance(nodes.begin(), nodes.find(node));
}

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

    // Build flat graph
    std::vector<JitPoint> jitPoints;
    jitPoints.resize(necessaryNodes.size());

    for(auto node : necessaryNodes)
    {
        size_t offset = FindNodeOffset(necessaryNodes, node);
        auto& jp = jitPoints[offset];
        jp.mNode = node;

        for(auto pnode : node->mParents)
        {
            auto& parent = jitPoints[FindNodeOffset(necessaryNodes, pnode)];
            jp.mParents.push_back(&parent);
            parent.mChildren.push_back(&jp);
        }

        if(node->mKind == Node::KIND_INPUT)
        {
            mInputPoints.push_back(Point());
            mInputs[node->mToken] = &mInputPoints.back();
            jp.mPoint = &mInputPoints.back();
        }
        if(node->mIsObserver)
        {
            mOutputPoints.push_back(Point());
            mObservers[node->mToken] = &mOutputPoints.back();
            jp.mPoint = &mOutputPoints.back();
        }
    }
    
    // Copy into computed heap
    std::sort(jitPoints.begin(), jitPoints.end());
    
    // JIT IT BABY
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    auto Owner = std::make_unique<llvm::Module>("exys", mLlvmContext);
    llvm::Module *M = Owner.get();
  
    std::vector<llvm::Type*> args;

    mStabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction("StabilizeFunc", 
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(mLlvmContext), // void return
                        args,
                        false))); // no var args

    auto *BB = llvm::BasicBlock::Create(mLlvmContext, "StabilizeBlock", mStabilizeFunc);

    llvm::IRBuilder<> builder(BB);

    for(auto& jp : jitPoints)
    {
        const_cast<JitPoint&>(jp).mValue = JitNode(builder, jp);
    }

    builder.CreateRetVoid();
    std::string error;
    mLlvmExecEngine = llvm::EngineBuilder(std::move(Owner))
                                .setEngineKind(llvm::EngineKind::JIT)
                                .setErrorStr(&error)
                                .create();
    if(!mLlvmExecEngine)
    {
        // throw here
        std::cout << error;
        assert(mLlvmExecEngine);
    }
    mRawStabilizeFunc = (void(*)()) mLlvmExecEngine->getPointerToFunction(mStabilizeFunc);
    mLlvmExecEngine->finalizeObject();

    llvm::outs() << "We just constructed this LLVM module:\n\n" << *M;

    Stabilize();
}

bool Jitter::IsDirty()
{
    return mDirty;
}

void Jitter::Stabilize()
{
    if(mDirty)
    {
        if(mRawStabilizeFunc)
        {
            mRawStabilizeFunc();
        }
        else
        {
            std::vector<llvm::GenericValue> noargs;
            mLlvmExecEngine->runFunction(mStabilizeFunc, noargs);
        }
        mDirty = false;
    }
}

void Jitter::PointChanged(Point&)
{
    mDirty = true;
}

bool Jitter::HasInputPoint(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& Jitter::LookupInputPoint(const std::string& label)
{
    assert(HasInputPoint(label));
    auto niter = mInputs.find(label);
    return *niter->second;
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
        ret[ip.first] = ip.second->mD;
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
    assert(HasObserverPoint(label));
    auto niter = mObservers.find(label);
    return *niter->second;
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
        ret[ip.first] = ip.second->mD;
    }
    return ret;
}

std::unique_ptr<Graph> BuildAndLoadGraph()
{
    auto graph = std::make_unique<Graph>();
    std::vector<Procedure> procedures;
    for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
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

