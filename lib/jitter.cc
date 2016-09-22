#ifndef _WIN32

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

static void DummyValidator(Node::Ptr)
{
}

llvm::Value* JitTernary(llvm::IRBuilder<>& builder, const JitPoint& point)
{
    assert(point.mParents.size() == 3);
    llvm::Value* cmp = point.mParents[0]->mValue;
    if(cmp->getType() == builder.getDoubleTy())
    {
        llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
        cmp = builder.CreateFCmpONE(point.mParents[0]->mValue, zero);
    }

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

#define DEFINE_PAIR_OPERATOR(__FUNCNAME, __MEMFUNC) \
llvm::Value* __FUNCNAME(llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    assert(point.mParents.size() == 2); \
    return builder.__MEMFUNC(point.mParents[0]->mValue, point.mParents[1]->mValue); \
}

DEFINE_LOOP_OPERATOR(JitDoubleLT,  CreateFCmpOLT);
DEFINE_LOOP_OPERATOR(JitDoubleLE,  CreateFCmpOLE);
DEFINE_LOOP_OPERATOR(JitDoubleGT,  CreateFCmpOGT);
DEFINE_LOOP_OPERATOR(JitDoubleGE,  CreateFCmpOGE);
DEFINE_LOOP_OPERATOR(JitDoubleEQ,  CreateFCmpOEQ);
DEFINE_LOOP_OPERATOR(JitDoubleNE,  CreateFCmpONE);
DEFINE_LOOP_OPERATOR(JitDoubleAnd, CreateAnd);
DEFINE_LOOP_OPERATOR(JitDoubleOr,  CreateOr);

static JitPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",    DummyValidator},  JitTernary},
    {{"+",    DummyValidator},  JitDoubleAdd},
    {{"-",    DummyValidator},  JitDoubleSub},
    {{"/",    DummyValidator},  JitDoubleDiv},
    {{"*",    DummyValidator},  JitDoubleMul},
    ////{{"%",    DummyValidator},  LoopOperator<std::modulus<double>>},
    {{"<",    DummyValidator},  JitDoubleLT},
    {{"<=",   DummyValidator},  JitDoubleLE},
    {{">",    DummyValidator},  JitDoubleGT},
    {{">=",   DummyValidator},  JitDoubleGE},
    {{"==",   DummyValidator},  JitDoubleEQ},
    {{"!=",   DummyValidator},  JitDoubleNE},
    {{"&&",   DummyValidator},  JitDoubleAnd},
    {{"||",   DummyValidator},  JitDoubleOr},
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
        // calling this on dealloc causes the next 
        // instance to blow up
        //llvm::llvm_shutdown();
    }
}

std::string Jitter::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
}

std::string Jitter::GetLlvmIR()
{
    std::string out;
    llvm::raw_string_ostream rawout(out);

    rawout << *mStabilizeFunc;

    return rawout.str();
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
    auto* ptrAsInt = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*mLlvmContext), (uintptr_t)&point.mVal);
    return llvm::ConstantExpr::getIntToPtr(ptrAsInt, llvm::Type::getDoublePtrTy(*mLlvmContext));
}

llvm::Value* Jitter::JitNode(llvm::IRBuilder<>& builder, const JitPoint& jp)
{
    llvm::Value* ret = nullptr;
    if(jp.mNode->mKind == Node::KIND_CONST)
    {
      ret = llvm::ConstantFP::get(builder.getDoubleTy(), std::stod(jp.mNode->mToken));
    }
    else if(jp.mNode->mIsInput)
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
        if(ret->getType() != builder.getDoubleTy())
        {
            ret = builder.CreateUIToFP(ret, builder.getDoubleTy());
        }
        builder.CreateStore(ret, GetPtrForPoint(*jp.mPoint));
    }
    return ret;
}

static size_t FindNodeOffset(const std::set<Node::Ptr>& nodes, Node::Ptr node)
{
    return std::distance(nodes.begin(), nodes.find(node));
}

struct CmpJitPointPtr
{
    bool operator()(const JitPoint* lhs, const JitPoint* rhs) const
    {
        const auto height = lhs->mNode->mHeight;
        const auto rhsHeight = rhs->mNode->mHeight;
        return (height > rhsHeight) || ((height == rhsHeight) && (lhs > rhs));
    }
};

void Jitter::CompleteBuild()
{
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

    // Build flat graph
    std::vector<JitPoint> jitPoints;
    jitPoints.resize(necessaryNodes.size());
    int incnt = 0;
    int outcnt = 0;
    for(auto node : necessaryNodes)
    {
        if(node->mIsInput)
        {
            ++incnt;
        }
        if(node->mIsObserver)
        {
            ++outcnt;
        }
    }

    mInputPoints.resize(incnt);
    mOutputPoints.resize(outcnt);

    auto inpoint = mInputPoints.begin();
    auto outpoint = mOutputPoints.begin();

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

        std::unordered_map<Node::Ptr, std::string>::iterator ob;
        if(node->mIsInput)
        {
            mInputs[node->mToken] = &(*inpoint);
            jp.mPoint = &(*inpoint);
            ++inpoint;
        }
        if((ob = observers.find(node)) != observers.end())
        {
            mObservers[ob->second] = &(*outpoint);
            jp.mPoint = &(*outpoint);
            ++outpoint;
        }
    }
    
    // Copy into computed heap
    std::set<JitPoint*, CmpJitPointPtr> jitHeap;
    for(auto& jp : jitPoints)
    {
        jitHeap.insert(&jp);
    }
    
    // JIT IT BABY
    mLlvmContext = new llvm::LLVMContext;
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    auto Owner = std::make_unique<llvm::Module>("exys", *mLlvmContext);
    llvm::Module *M = Owner.get();
  
    std::vector<llvm::Type*> args;

    mStabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction("StabilizeFunc", 
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        args,
                        false))); // no var args

    auto *BB = llvm::BasicBlock::Create(*mLlvmContext, "StabilizeBlock", mStabilizeFunc);

    llvm::IRBuilder<> builder(BB);

    for(auto& jp : jitHeap)
    {
        const_cast<JitPoint*>(jp)->mValue = JitNode(builder, *jp);
    }

    builder.CreateRetVoid();
    std::string error;
    mLlvmExecEngine = llvm::EngineBuilder(std::move(Owner))
                                .setEngineKind(llvm::EngineKind::JIT)
                                .setOptLevel(llvm::CodeGenOpt::Level::Aggressive)
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

    //std::cout << GetLlvmIR();

    Stabilize();
}

bool Jitter::IsDirty()
{
    for(const auto& p : mInputPoints) if(p.mDirty) return true;
    return false;
}

void Jitter::Stabilize()
{
    if(IsDirty())
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
        for(auto& p : mInputPoints) p.mDirty = false;
    }
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
        ret[ip.first] = ip.second->mVal;
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
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

static std::unique_ptr<Graph> BuildAndLoadGraph()
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
    graph->Construct(Parse(text));
    auto engine = std::make_unique<Jitter>(std::move(graph));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

#endif
