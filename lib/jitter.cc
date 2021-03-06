#ifndef _WIN32

#include <iostream>
#include <sstream>
#include <cassert>

#include "llvm/IR/Intrinsics.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"

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

llvm::Value* JitTernary(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    llvm::Value* cmp = point.mParents[0]->mValue;
    if(cmp->getType() == builder.getDoubleTy())
    {
        llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
        cmp = builder.CreateFCmpUNE(point.mParents[0]->mValue, zero);
    }

    return builder.CreateSelect(cmp,
            point.mParents[1]->mValue,
            point.mParents[2]->mValue);
}

llvm::Value* JitDoubleNot(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    llvm::Value* cmp = point.mParents[0]->mValue;
    llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0);
    llvm::Value* one = llvm::ConstantFP::get(builder.getDoubleTy(), 1.0);
    if(cmp->getType() == builder.getDoubleTy())
    {
        cmp = builder.CreateFCmpUNE(point.mParents[0]->mValue, zero);
    }

    return builder.CreateSelect(cmp, zero, one);
}

llvm::Value* Jitter::JitGV(llvm::Module* M, llvm::IRBuilder<>& builder)
{
    llvm::Value* stateIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), mNumStatePtr++);
    std::vector<llvm::Value*> gepIndex;
    gepIndex.push_back(stateIndex);
    return builder.CreateGEP(mStatePtr, gepIndex);
}

llvm::Value* Jitter::JitLoad(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    return builder.CreateLoad(point.mParents[0]->mValue);
}

llvm::Value* Jitter::JitStore(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    llvm::Value* dst = point.mParents[0]->mValue;
    llvm::Value* src = point.mParents[1]->mValue;

    builder.CreateStore(src, dst);
    return src;
}

llvm::Value* Jitter::JitTick(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    auto* gv = JitGV(M, builder);

    llvm::Value* one = llvm::ConstantFP::get(builder.getDoubleTy(), 1.0);

    auto* loadGv = builder.CreateLoad(gv);
    auto* ret = builder.CreateFAdd(loadGv, one);
    builder.CreateStore(ret, gv);
    return ret;
}

llvm::Value* JitNull(llvm::Module* M, llvm::IRBuilder<>& builder, const JitPoint& point)
{
    return nullptr;
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
DEFINE_INTRINSICS_UNARY_OPERATOR(JitDoubleTrunc, trunc);

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
        val = (builder.__MEMFUNC)(builder.CreateSIToFP(val,builder.getDoubleTy()), \
            builder.CreateSIToFP((*p)->mValue,builder.getDoubleTy())); \
    } \
    return val; \
}

DEFINE_LOOP_OPERATOR(JitDoubleAdd, CreateFAdd);
DEFINE_LOOP_OPERATOR(JitDoubleSub, CreateFSub);
DEFINE_LOOP_OPERATOR(JitDoubleMul, CreateFMul);
DEFINE_LOOP_OPERATOR(JitDoubleDiv, CreateFDiv);
DEFINE_LOOP_OPERATOR(JitDoubleMod, CreateFRem);
DEFINE_LOOP_OPERATOR(JitDoubleLT,  CreateFCmpOLT);
DEFINE_LOOP_OPERATOR(JitDoubleLE,  CreateFCmpOLE);
DEFINE_LOOP_OPERATOR(JitDoubleGT,  CreateFCmpOGT);
DEFINE_LOOP_OPERATOR(JitDoubleGE,  CreateFCmpOGE);
DEFINE_LOOP_OPERATOR(JitDoubleEQ,  CreateFCmpOEQ);
DEFINE_LOOP_OPERATOR(JitDoubleNE,  CreateFCmpUNE);

// Here we compare all doubles with zero before combining them logically
#define DEFINE_LOGICAL_LOOP_OPERATOR(__FUNCNAME, __MEMFUNC) \
llvm::Value* __FUNCNAME(llvm::Module*, llvm::IRBuilder<>& builder, const JitPoint& point) \
{ \
    assert(point.mParents.size() >= 2); \
    auto p = point.mParents.begin(); \
    llvm::Value *val = (*p)->mValue; \
    llvm::Value* zero = llvm::ConstantFP::get(builder.getDoubleTy(), 0.0); \
    if(val->getType() == builder.getDoubleTy()) \
    { \
        val = builder.CreateFCmpUNE(val, zero); \
    } \
    for(++p; p != point.mParents.end(); ++p) \
    { \
        llvm::Value *other = (*p)->mValue; \
        if(other->getType() == builder.getDoubleTy()) \
        { \
            other = builder.CreateFCmpUNE(other, zero); \
        } \
        assert(val); \
        assert((*p)->mValue); \
        val = (builder.__MEMFUNC)(val, other); \
    } \
    return val; \
}

DEFINE_LOGICAL_LOOP_OPERATOR(JitDoubleAnd, CreateAnd);
DEFINE_LOGICAL_LOOP_OPERATOR(JitDoubleOr,  CreateOr);

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

void SimApplyValid(Node::Ptr node)
{
    auto args = node->mParents;
    if(args.size() != 3)
    {
        std::stringstream err;
        err << "Incorrect number of args. Got " << args.size() <<
            " Expected 3";
        throw GraphBuildException(err.str(), Cell());
    }

    ValidateArgsNotNull(node);

    auto target = std::static_pointer_cast<Node>(args[0]);
    auto overwrite = std::static_pointer_cast<Node>(args[1]);
    auto doneFlag = std::static_pointer_cast<Node>(args[2]);

    if(!(overwrite->mKind & (Node::KIND_CONST|Node::KIND_BIND|Node::KIND_LIST|Node::KIND_PROC)))
    {
        std::stringstream err;
        err << "Incorrect overwrite argument type for function." <<
            " Got " << overwrite->mKind << ". Expected Const, Var, List";
        throw GraphBuildException(err.str(), Cell());
    }

    if((target->mKind == Node::KIND_LIST) && (target->mKind != overwrite->mKind))
    {
        std::stringstream err;
        err << "Overwrite did not match target kind for '" << target->mToken << 
            "'. Expected kind " << target->mKind << " Got " << overwrite->mKind;
        throw GraphBuildException(err.str(), Cell());
    }

    if((target->mKind == Node::KIND_LIST) && target->mParents.size() != overwrite->mParents.size())
    {
        std::stringstream err;
        err << "Overwrite incorrect size for target '" << target->mToken << 
            "'. Expected size " << target->mParents.size() << " Got " << overwrite->mParents.size();
        throw GraphBuildException(err.str(), Cell());
    }
}

#define WRAP(__FUNC) \
    [this](llvm::Module* m, llvm::IRBuilder<>& b, const JitPoint& p) -> llvm::Value* \
    {return this->__FUNC(m,b,p);}

static JitPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",         CountValueValidator<3,3>},   JitTernary},
    {{"+",         MinCountValueValidator<2>},  JitDoubleAdd},
    {{"-",         MinCountValueValidator<2>},  JitDoubleSub},
    {{"/",         MinCountValueValidator<2>},  JitDoubleDiv},
    {{"*",         MinCountValueValidator<2>},  JitDoubleMul},
    {{"%",         MinCountValueValidator<2>},  JitDoubleMod},
    {{"<",         CountValueValidator<2,2>},   JitDoubleLT},
    {{"<=",        CountValueValidator<2,2>},   JitDoubleLE},
    {{">",         CountValueValidator<2,2>},   JitDoubleGT},
    {{">=",        CountValueValidator<2,2>},   JitDoubleGE},
    {{"==",        CountValueValidator<2,2>},   JitDoubleEQ},
    {{"!=",        CountValueValidator<2,2>},   JitDoubleNE},
    {{"&&",        MinCountValueValidator<2>},  JitDoubleAnd},
    {{"||",        MinCountValueValidator<2>},  JitDoubleOr},
    {{"min",       MinCountValueValidator<2>},  JitDoubleMin},
    {{"max",       MinCountValueValidator<2>},  JitDoubleMax},
    {{"trunc",     CountValueValidator<1,1>},   JitDoubleTrunc},
    {{"exp",       CountValueValidator<1,1>},   JitDoubleExp},
    {{"ln",        CountValueValidator<1,1>},   JitDoubleLn},
    {{"not",       CountValueValidator<1,1>},   JitDoubleNot},
    {{"copy",      CountValueValidator<1,1>},   JitCopy},
    {{"sim-apply", SimApplyValid},              JitCopy}
};

Jitter::Jitter()
{
    for(auto& jpp : AVAILABLE_PROCS)
    {
        mPointProcessors.push_back(jpp);
    }
    mPointProcessors.push_back({{"store",      CountValueValidator<2,2>},   WRAP(JitStore)});
    mPointProcessors.push_back({{"load",       CountValueValidator<1,1>},   WRAP(JitLoad)});
    mPointProcessors.push_back({{"tick",       MinCountValueValidator<0>},  WRAP(JitTick)});
}
    //auto OwnerClone = std::unique_ptr<llvm::Module>(llvm::CloneModule(M));

Jitter::~Jitter()
{
    // dealloc engines here but
    // calling this on dealloc causes the next 
    // instance to blow up
    //llvm::llvm_shutdown();
}

std::string Jitter::GetDOTGraph() const
{
    return "digraph " + mGraph->GetDOTGraph();
}

llvm::Value* Jitter::JitNode(llvm::Module* M, llvm::IRBuilder<>&  builder, 
        const JitPoint& jp, llvm::Value* inputs, llvm::Value* observers)
{
    llvm::Value* ret = nullptr;
    llvm::Value* valIndex   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), 0);
    llvm::Value* dirtyIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), 2); 

    llvm::Value* dirtyFlag = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*mLlvmContext), 1);

    if(jp.mNode->mKind == Node::KIND_CONST)
    {
        ret = llvm::ConstantFP::get(builder.getDoubleTy(), std::stod(jp.mNode->mToken));
    }
    else if(jp.mNode->mKind == Node::KIND_VAR)
    {
        mStateInitializers.push_back(
            std::make_pair(mNumStatePtr, jp.mNode->mInitValue));
        ret = JitGV(M, builder);
    }
    else if(jp.mNode->mInputOffset >= 0)
    {
        std::vector<llvm::Value*> gepIndex;
        llvm::Value* nodeOffset = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), jp.mNode->mInputOffset);
        gepIndex.push_back(nodeOffset);
        gepIndex.push_back(valIndex);
        llvm::Value* point = builder.CreateGEP(inputs, gepIndex);
        ret = builder.CreateLoad(point);
    }
    else if(jp.mNode->mKind == Node::KIND_PROC)
    {
        for(auto& proc : mPointProcessors)
        {
            if(jp.mNode->mToken.compare(proc.procedure.id) == 0)
            {
                assert(std::all_of(jp.mParents.begin(), jp.mParents.end(), [](JitPoint* p){return p->mValue != 0;}));
                ret = proc.func(M, builder, jp);
            }
        }
    }

    assert(ret);

    if(jp.mNode->mObserverOffset >= 0)
    {
        if(ret->getType() != builder.getDoubleTy())
        {
            ret = builder.CreateUIToFP(ret, builder.getDoubleTy());
        }

        std::vector<llvm::Value*> gepIndex;
        llvm::Value* nodeOffset = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), jp.mNode->mObserverOffset);
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
    auto foundNode = std::find(std::begin(nodes), std::end(nodes), node);
    assert(foundNode != nodes.end() && "Could not find node");
    return std::distance(nodes.begin(), foundNode);
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

llvm::PointerType* GetPointPointerType(llvm::Module* M)
{
    // before changing this type 
    // review exys.h header where the type matches
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

    return llvm::PointerType::get(pointType, 0 /*address space*/);
}

std::unique_ptr<llvm::Module> Jitter::BuildModule()
{    
    if(mLlvmContext)
    {
        assert(!mLlvmContext && "Do not call build module multiple times - Call clone module if you want a copy");
        return nullptr;
    }

    auto sims = mGraph->SplitOutBy(Node::KIND_PROC, "sim-apply");

    // JIT IT BABY
    mLlvmContext = new llvm::LLVMContext;

    auto module = std::unique_ptr<llvm::Module>(new llvm::Module("exys", *mLlvmContext));
    llvm::Module *M = module.get();

    llvm::PointerType* pointerToPoint = GetPointPointerType(M);
    llvm::PointerType* pointerToDouble = llvm::PointerType::get(llvm::Type::getDoubleTy(M->getContext()), 0 /*address space*/);

    // Create valuation function
    std::vector<llvm::Type*> inoutargs;
    inoutargs.push_back(pointerToPoint);  // inputs
    inoutargs.push_back(pointerToPoint);  // observers
    inoutargs.push_back(pointerToDouble); // state variables
  
    auto stabFuncName = MangleName(STAB_FUNC_NAME, M->getDataLayout());
    auto* stabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction(stabFuncName,
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        inoutargs,
                        false))); // no var args

    llvm::Function::arg_iterator args = stabilizeFunc->arg_begin();
    llvm::Value* inputsPtr = &(*args++);
    llvm::Value* observersPtr = &(*args++);
    llvm::Value* statePtr = &(*args++);

    auto nodeLayout = mGraph->GetLayout();
    llvm::IRBuilder<> mainBuilder(BuildBlock(STAB_FUNC_NAME, nodeLayout, stabilizeFunc, M, inputsPtr, observersPtr, statePtr));
    mainBuilder.CreateRetVoid();

    // Record inputs and outputs
    for(auto node : nodeLayout)
    {
        if(node->mInputOffset >= 0)
        {
            mInputs.push_back(node);
        }
        if(node->mObserverOffset >= 0)
        {
            mObservers.push_back(node);
        }
    }

    // Create sim function
    inoutargs.push_back(llvm::Type::getInt32Ty(M->getContext())); // sim function id
    auto simFuncName = MangleName(SIM_FUNC_NAME, M->getDataLayout());
    auto* simFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction(simFuncName,
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        inoutargs,
                        false))); // no var args

    llvm::Function::arg_iterator simargs = simFunc->arg_begin();
    llvm::Value* simInputsPtr = &(*simargs++);
    llvm::Value* simObserversPtr = &(*simargs++);
    llvm::Value* simStatePtr = &(*simargs++);
    llvm::Value* simId = &(*simargs++);

    if (sims.size())
    {
        auto *entry = llvm::BasicBlock::Create(*mLlvmContext, "switch-entry", simFunc);
        auto *switchBlock = llvm::BasicBlock::Create(*mLlvmContext, "sim-switch", simFunc);
        auto *end = llvm::BasicBlock::Create(*mLlvmContext, "switch-end", simFunc);
        llvm::IRBuilder<> switchBuilder(entry);
        switchBuilder.CreateBr(switchBlock);
        switchBuilder.SetInsertPoint(switchBlock);
        auto swinstr = switchBuilder.CreateSwitch(simId, switchBlock, sims.size());
        for(auto& sim : sims)
        {
            mSimTargets.push_back(sim->GetSimApplyTarget());

            auto* id = llvm::ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), mNumSimFunc++);
            auto layout = sim->GetSimApplyLayout();
            auto* block = BuildBlock("sim-switch", layout, simFunc, M, simInputsPtr, simObserversPtr, simStatePtr, true);
            switchBuilder.SetInsertPoint(block);
            switchBuilder.CreateBr(end);
            swinstr->addCase(id, block);

        }
        switchBuilder.SetInsertPoint(end);
        switchBuilder.CreateRetVoid();
    }
    else
    {
        auto *end = llvm::BasicBlock::Create(*mLlvmContext, "switch-end", simFunc);
        llvm::IRBuilder<> endBuilder(end);
        endBuilder.CreateRetVoid();
    }

    // Create initialization function
    auto initFuncName = MangleName(INIT_FUNC_NAME, M->getDataLayout());
    std::vector<llvm::Type*> initArgsDef;
    initArgsDef.push_back(pointerToDouble); // state variables
    auto* initFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction(initFuncName,
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(*mLlvmContext), // void return
                        initArgsDef,
                        false))); // no var args

    llvm::Function::arg_iterator initArgs = initFunc->arg_begin();
    llvm::Value* initStatePtr = &(*initArgs);

    auto *initEntry = llvm::BasicBlock::Create(*mLlvmContext, "init-entry", initFunc);
    llvm::IRBuilder<> initBuilder(initEntry);
    for(const auto initPairs : mStateInitializers)
    {
        llvm::Value* val = llvm::ConstantFP::get(initBuilder.getDoubleTy(), initPairs.second);
        llvm::Value* stateIndex = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*mLlvmContext), initPairs.first);
        std::vector<llvm::Value*> gepIndex;
        gepIndex.push_back(stateIndex);
        auto* addr = initBuilder.CreateGEP(initStatePtr, gepIndex);
        initBuilder.CreateStore(val, addr);
    }
    initBuilder.CreateRetVoid();

    // Optimisation passes
    llvm::legacy::PassManager *PM2 = new llvm::legacy::PassManager();
    llvm::legacy::FunctionPassManager *FM = new llvm::legacy::FunctionPassManager(module.get());

    llvm::PassManagerBuilder PMB;
    PMB.OptLevel = 3;
    PMB.DisableUnitAtATime = false;
    PMB.DisableUnrollLoops = false;
    PMB.BBVectorize = true;;
    PMB.SLPVectorize = true;;
    PMB.SLPVectorize = true;;
    PMB.LoopVectorize = true;;
    PMB.RerollLoops = true;
    PMB.LoadCombine = true;
    PMB.DisableGVNLoadPRE = true;
    PMB.VerifyInput = true;
    PMB.VerifyOutput = true;
    PMB.MergeFunctions = true;
    PMB.PrepareForLTO = false;
    PMB.Inliner = llvm::createFunctionInliningPass();
    PMB.populateModulePassManager(*PM2);
    PMB.populateFunctionPassManager(*FM);

    //llvm::DebugFlag=true;
    FM->doInitialization();
    for (llvm::Function &F : *module.get()) 
    {
        FM->run(F);
    }
    FM->doFinalization();
    PM2->run(*module.get());
 
    // Output asm
    std::string out;
    llvm::raw_string_ostream rawout(out);
    if(llvm::verifyModule(*M, &rawout))
    {
        rawout << *stabilizeFunc;
        rawout << *simFunc;
        rawout << *initFunc;
        throw GraphBuildException(rawout.str(), Cell());
    }
    return module;
}

llvm::BasicBlock* Jitter::BuildBlock(const std::string& blockName, const std::vector<Node::Ptr>& nodeLayout, 
        llvm::Function* func, llvm::Module *M, llvm::Value* inputsPtr, llvm::Value* observersPtr, llvm::Value* statePtr,
        bool ret)
{
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
    }

    auto *block = llvm::BasicBlock::Create(*mLlvmContext, blockName, func);
    llvm::IRBuilder<> builder(block);
    mStatePtr = statePtr;
    mNumStatePtr = 0;

    // Copy into computed heap
    std::set<JitPoint*, CmpJitPointPtr> jitHeap;
    for(auto& jp : jitPoints)
    {
        assert(jp.mNode);
        jitHeap.insert(&jp);
    }
    for(auto& jp : jitHeap)
    {
        const_cast<JitPoint*>(jp)->mValue = JitNode(M, builder, *jp, inputsPtr, observersPtr);
    }

    return block;
}
    
std::unique_ptr<Graph> Jitter::BuildAndLoadGraph()
{
    auto graph = std::unique_ptr<Graph>(new Graph);
    std::vector<Procedure> procedures;
    for(const auto &proc : mPointProcessors) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

void Jitter::AssignGraph(std::unique_ptr<Graph>& graph)
{
    mGraph.swap(graph);
}

std::unique_ptr<Jitter> Jitter::Build(const std::string& text)
{
    auto jitter = std::unique_ptr<Jitter>(new Jitter());
    auto graph = jitter->BuildAndLoadGraph();

    graph->Construct(Parse(text));
    jitter->AssignGraph(graph);
    return jitter;
}

}

#endif
