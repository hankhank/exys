#ifndef _WIN32

#include <iostream>
#include <sstream>
#include <cassert>

#include "llvm/IR/Intrinsics.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "jitter.h"
#include "helpers.h"

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

llvm::GlobalVariable* JitGV(llvm::Module* M, llvm::IRBuilder<>& builder)
{
    auto* gv = new llvm::GlobalVariable(*M, builder.getDoubleTy(), false, llvm::GlobalValue::InternalLinkage,
                     llvm::ConstantFP::get(builder.getDoubleTy(), 0.0), "", nullptr,
                     llvm::GlobalValue::ThreadLocalMode::NotThreadLocal, 
                     5); //llvm::AddressSpace::ADDRESS_SPACE_LOCAL /* for cuda kernel */);
    return gv;
}

llvm::Value* JitTernary(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point)
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

llvm::Value* JitDoubleNot(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    assert(point.mParents.size() == 1);
    llvm::Value* cmp = point.mParents[0]->mValue;
    llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
    llvm::Value* one = llvm::ConstantFP::get(builder.getDoubleTy(), 1.0);
    if(cmp->getType() == builder.getDoubleTy())
    {
        cmp = builder.CreateFCmpONE(point.mParents[0]->mValue, zero);
    }

    return builder.CreateSelect(cmp, zero, one);
}

llvm::Value* JitLatch(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    assert(point.mParents.size() == 2);

    auto* gv = JitGV(M, builder);

    llvm::Value* cmp = point.mParents[0]->mValue;
    llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
    if(cmp->getType() == builder.getDoubleTy())
    {
        cmp = builder.CreateFCmpONE(point.mParents[0]->mValue, zero);
    }
    
    auto* loadGv = builder.CreateLoad(gv);
    auto* ret = builder.CreateSelect(cmp, point.mParents[1]->mValue, loadGv);
    builder.CreateStore(ret, gv);
    return ret;
}

llvm::Value* JitFlipFlop(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    assert(point.mParents.size() == 2);

    auto* gv = JitGV(M, builder);

    llvm::Value* cmp = point.mParents[0]->mValue;
    llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
    if(cmp->getType() == builder.getDoubleTy())
    {
        cmp = builder.CreateFCmpONE(point.mParents[0]->mValue, zero);
    }
    
    auto* loadGv = builder.CreateLoad(gv);
    auto* ret = builder.CreateSelect(cmp, point.mParents[1]->mValue, loadGv);
    builder.CreateStore(ret, gv);
    return loadGv;
}

llvm::Value* JitTick(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    assert(point.mParents.size() == 2);

    auto* gv = JitGV(M, builder);

    llvm::Value* one = llvm::ConstantFP::get(builder.getDoubleTy(), 1.0);

    auto* loadGv = builder.CreateLoad(gv);
    auto* ret = builder.CreateFAdd(loadGv, one);
    builder.CreateStore(ret, gv);
    return ret;
}

#define DEFINE_INTRINSICS_UNARY_OPERATOR(__FUNCNAME, __INFUNC) \
llvm::Value* __FUNCNAME(llvm::Module *M, llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    std::vector<llvm::Type*> argTypes; \
    std::vector<llvm::Value*> argValues; \
    auto p = point.mParents.begin(); \
    argTypes.push_back(builder.getDoubleTy()); \
    argValues.push_back((*p)->mValue); \
    llvm::Function *fun = llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::__INFUNC, argTypes); \
    return builder.CreateCall(fun, argValues); \
}

DEFINE_INTRINSICS_UNARY_OPERATOR(JitDoubleExp, exp);
DEFINE_INTRINSICS_UNARY_OPERATOR(JitDoubleLn, log);

#define DEFINE_INTRINSICS_LOOP_OPERATOR(__FUNCNAME, __INFUNC) \
llvm::Value* __FUNCNAME(llvm::Module *M, llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    auto p = point.mParents.begin(); \
    llvm::Value *val = (*p)->mValue; \
    for(++p; p != point.mParents.end(); ++p) \
    { \
        assert((*p)->mValue); \
        std::vector<llvm::Type*> argTypes; \
        std::vector<llvm::Value*> argValues; \
        argTypes.push_back(builder.getDoubleTy()); \
        argValues.push_back(val); \
        argValues.push_back((*p)->mValue); \
        llvm::Function *fun = llvm::Intrinsic::getDeclaration(M, llvm::Intrinsic::__INFUNC, argTypes); \
        val = builder.CreateCall(fun, argValues); \
    } \
    return val; \
}

DEFINE_INTRINSICS_LOOP_OPERATOR(JitDoubleMin, minnum);
DEFINE_INTRINSICS_LOOP_OPERATOR(JitDoubleMax, maxnum);

#define DEFINE_LOOP_OPERATOR(__FUNCNAME, __MEMFUNC) \
llvm::Value* __FUNCNAME(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    assert(point.mParents.size() >= 2); \
    auto p = point.mParents.begin(); \
    llvm::Value *val = (*p)->mValue; \
    for(++p; p != point.mParents.end(); ++p) \
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
DEFINE_LOOP_OPERATOR(JitDoubleLT,  CreateFCmpOLT);
DEFINE_LOOP_OPERATOR(JitDoubleLE,  CreateFCmpOLE);
DEFINE_LOOP_OPERATOR(JitDoubleGT,  CreateFCmpOGT);
DEFINE_LOOP_OPERATOR(JitDoubleGE,  CreateFCmpOGE);
DEFINE_LOOP_OPERATOR(JitDoubleEQ,  CreateFCmpOEQ);
DEFINE_LOOP_OPERATOR(JitDoubleNE,  CreateFCmpONE);
DEFINE_LOOP_OPERATOR(JitDoubleAnd, CreateAnd);
DEFINE_LOOP_OPERATOR(JitDoubleOr,  CreateOr);

#define DEFINE_UNARY_OPERATOR(__FUNCNAME, __MEMFUNC) \
llvm::Value* __FUNCNAME(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    assert(point.mParents.size() == 1); \
    return builder.__MEMFUNC(point.mParents[0]->mValue); \
}

llvm::Value* JitCopy(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    auto p = point.mParents.begin();
    return (*p)->mValue;
}

typedef std::function<llvm::Value* (llvm::Module*, llvm::IRBuilder<>&, const JitPoint&)> ComputeFunction;

struct JitPointProcessor
{
    Procedure procedure;
    ComputeFunction func; 
};

static JitPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",         CountValueValidator<3,3>},   JitTernary},
    {{"+",         MinCountValueValidator<2>},  JitDoubleAdd},
    {{"-",         MinCountValueValidator<2>},  JitDoubleSub},
    {{"/",         MinCountValueValidator<2>},  JitDoubleDiv},
    {{"*",         MinCountValueValidator<2>},  JitDoubleMul},
    ////{{"%",       DummyValidator},Wrap(  LoLoopOperator<std::modulus<double>>},
    {{"<",         CountValueValidator<2,2>},   JitDoubleLT},
    {{"<=",        CountValueValidator<2,2>},   JitDoubleLE},
    {{">",         CountValueValidator<2,2>},   JitDoubleGT},
    {{">=",        CountValueValidator<2,2>},   JitDoubleGE},
    {{"==",        CountValueValidator<2,2>},   JitDoubleEQ},
    {{"!=",        CountValueValidator<2,2>},   JitDoubleNE},
    {{"&&",        CountValueValidator<2,2>},   JitDoubleAnd},
    {{"||",        CountValueValidator<2,2>},   JitDoubleOr},
    {{"min",       MinCountValueValidator<2>},  JitDoubleMin},
    {{"max",       MinCountValueValidator<2>},  JitDoubleMax},
    {{"exp",       CountValueValidator<1,1>},   JitDoubleExp},
    {{"ln",        CountValueValidator<1,1>},   JitDoubleLn},
    {{"not",       CountValueValidator<1,1>},   JitDoubleNot},
    {{"latch",     CountValueValidator<2,2>},   JitLatch},
    {{"flip-flop", CountValueValidator<2,2>},   JitFlipFlop},
    {{"tick",      MinCountValueValidator<0>},  JitTick},
    {{"copy",      CountValueValidator<1,1>},   JitCopy}
};

Jitter::Jitter(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}
    //auto OwnerClone = std::unique_ptr<llvm::Module>(llvm::CloneModule(M));

Jitter::~Jitter()
{
    // dealloc engines here but
    // calling this on dealloc causes the next 
    // instance to blow up
    //llvm::llvm_shutdown();
}

std::string Jitter::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
}

llvm::Value* Jitter::JitNode(llvm::Module* M, llvm::IRBuilder<>&  builder, 
        const JitPoint& jp, llvm::Value* inputs, llvm::Value* observers)
{
    llvm::Value* ret = nullptr;
    llvm::Value* valIndex   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), 0);
    llvm::Value* dirtyIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), 2); 
    llvm::Value* nodeOffset = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), jp.mNode->mOffset);

    llvm::Value* dirtyFlag = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*mLlvmContext), 1);

    if(jp.mNode->mKind == Node::KIND_CONST)
    {
      ret = llvm::ConstantFP::get(builder.getDoubleTy(), std::stod(jp.mNode->mToken));
    }
    else if(jp.mNode->mIsInput)
    {
        std::vector<llvm::Value*> gepIndex;
        gepIndex.push_back(nodeOffset);
        gepIndex.push_back(valIndex);
        llvm::Value* point = builder.CreateGEP(inputs, gepIndex);
        ret = builder.CreateLoad(point);
    }
    else if(jp.mNode->mKind == Node::KIND_PROC)
    {
        for(auto& proc : AVAILABLE_PROCS)
        {
            if(jp.mNode->mToken.compare(proc.procedure.id) == 0)
            {
                ret = proc.func(M, builder, jp);
            }
        }
        assert(ret);
    }

    if(jp.mNode->mIsObserver)
    {
        assert(ret);
        if(ret->getType() != builder.getDoubleTy())
        {
            ret = builder.CreateUIToFP(ret, builder.getDoubleTy());
        }

        std::vector<llvm::Value*> gepIndex;
        gepIndex.push_back(nodeOffset);
        gepIndex.push_back(valIndex);
        llvm::Value* observer = builder.CreateGEP(observers, gepIndex);
        builder.CreateStore(ret, observer);

        gepIndex.pop_back();
        gepIndex.push_back(dirtyIndex);
        llvm::Value* flag = builder.CreateGEP(observers, gepIndex);
        builder.CreateStore(dirtyFlag, flag);
    }
    return ret;
}

static size_t FindNodeOffset(const std::vector<Node::Ptr>& nodes, Node::Ptr node)
{
    return std::distance(nodes.begin(), std::find(std::begin(nodes), std::end(nodes), node));
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

std::string MangleName(const std::string& name, const llvm::DataLayout &DL)
{
    std::string mangledName;
    llvm::raw_string_ostream mangledNameStream(mangledName);
    llvm::Mangler::getNameWithPrefix(mangledNameStream, name, DL);
    return mangledNameStream.str();
}

std::unique_ptr<llvm::Module> Jitter::BuildModule()
{    
    if(mLlvmContext)
    {
        assert(!mLlvmContext && "Do not call build module multiple times - Call clone module if you want a copy");
        return nullptr;
    }

    auto nodeLayout = mGraph->GetLayout();

    std::vector<JitPoint> jitPoints;
    jitPoints.resize(nodeLayout.size());

    for(auto node : nodeLayout)
    {
        size_t offset = FindNodeOffset(nodeLayout, node);
        auto& jp = jitPoints[offset];
        jp.mNode = node;

        for(auto pnode : node->mParents)
        {
            auto& parent = jitPoints[FindNodeOffset(nodeLayout, pnode)];
            jp.mParents.push_back(&parent);
            parent.mChildren.push_back(&jp);
        }

        if(node->mIsInput)
        {
            mInputs.push_back(node);
        }
        if(node->mIsObserver)
        {
            mObservers.push_back(node);
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

    auto module = std::unique_ptr<llvm::Module>(new llvm::Module("exys", *mLlvmContext));
    llvm::Module *M = module.get();

    // before changing this type 
    //
    auto pointName = MangleName(POINT_NAME, M->getDataLayout());
    auto pointType = llvm::StructType::create(M->getContext(), pointName);
    std::vector<llvm::Type*> pointTypeFields;
    pointTypeFields.push_back(llvm::Type::getDoubleTy(M->getContext()));    // val
    pointTypeFields.push_back(llvm::IntegerType::get(M->getContext(), 32)); // length
    pointTypeFields.push_back(llvm::IntegerType::get(M->getContext(), 8));  // dirty
    pointTypeFields.push_back(llvm::IntegerType::get(M->getContext(), 8));  // char[0]
    pointTypeFields.push_back(llvm::IntegerType::get(M->getContext(), 8));  // char[1]
    pointTypeFields.push_back(llvm::IntegerType::get(M->getContext(), 8));  // char[2]
    pointType->setBody(pointTypeFields, /*isPacked=*/true);

    llvm::PointerType* pointerToPoint = llvm::PointerType::get(pointType, 0 /*address space*/);

    std::vector<llvm::Type*> inoutargs;
    inoutargs.push_back(pointerToPoint);
    inoutargs.push_back(pointerToPoint);
  
    auto stabFuncName = MangleName(STAB_FUNC_NAME, M->getDataLayout());
    auto* stabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction(stabFuncName,
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        inoutargs,
                        false))); // no var args

    auto *stabBlock = llvm::BasicBlock::Create(*mLlvmContext, "StabilizeBlock", stabilizeFunc);
    llvm::IRBuilder<> stabBuilder(stabBlock);

    llvm::Function::arg_iterator args = stabilizeFunc->arg_begin();
    llvm::Value* inputsPtr = &(*args++);
    llvm::Value* observersPtr = &(*args);
 
    for(auto& jp : jitHeap)
    {
        const_cast<JitPoint*>(jp)->mValue = JitNode(M, stabBuilder, *jp, inputsPtr, observersPtr);
    }
    stabBuilder.CreateRetVoid();

    std::vector<llvm::Type*> noargs;

    // Capture and reset state
    auto capFuncName = MangleName(CAP_FUNC_NAME, M->getDataLayout());
    auto* captureFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction(capFuncName, 
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        noargs,
                        false))); // no var args
    auto *capBlock = llvm::BasicBlock::Create(*mLlvmContext, "CaptureStateBlock", captureFunc);
    llvm::IRBuilder<> capBuilder(capBlock);

    auto resetFuncName = MangleName(RESET_FUNC_NAME, M->getDataLayout());
    auto* resetFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction(resetFuncName,
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        noargs,
                        false))); // no var args
    auto* resetBlock = llvm::BasicBlock::Create(*mLlvmContext, "ResetBlock", resetFunc);
    llvm::IRBuilder<> resetBuilder(resetBlock);

    // copy here so we aren't inserting new gv into the list we are working on
    std::vector<llvm::GlobalVariable*> gvList;
    for(auto& gv : module->getGlobalList())
    {
        gvList.push_back(&gv);
    }

    for(auto& gv : gvList)
    {
        auto* copy = JitGV(M, capBuilder);
        auto* loadedGv = capBuilder.CreateLoad(gv);
        capBuilder.CreateStore(loadedGv, copy);

        auto* loadCopy = resetBuilder.CreateLoad(copy);
        resetBuilder.CreateStore(loadCopy, gv);
    }
    capBuilder.CreateRetVoid();
    resetBuilder.CreateRetVoid();

    // Output asm
    if(0)
    {
        std::string out;
        llvm::raw_string_ostream rawout(out);

        rawout << *stabilizeFunc;
        rawout << *captureFunc;
        rawout << *resetFunc;

        std::cout << rawout.str();
    }
    
    return module;
}

static std::unique_ptr<Graph> BuildAndLoadGraph()
{
    auto graph = std::unique_ptr<Graph>(new Graph);
    std::vector<Procedure> procedures;
    for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

std::unique_ptr<Jitter> Jitter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Construct(Parse(text));
    return std::unique_ptr<Jitter>(new Jitter(std::move(graph)));
}

}

#endif
