
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <cmath>

#include "interpreter.h"
#include "helpers.h"

// Notes
// Handling list inputs and outputs is a bit of pain and not exactly symmetrical
// Concerns
// 1. To allow array like access we need to have all inputs and outputs
//    next to one another not randomly placed through out the graph
// 2. We need to add in extra mappings to the start of the arrays along from actual
//    array member usage
// 3. Observables need an extra copy incase list members exist in two output arrays.
//    We could handle the two cases avoiding the extra copy but its annoying.
// 4. We can't pass back Point to the user because they need special [] operators
//    to take into account the special interpoint type we use internally

namespace Exys
{

void ConstDummy(InterPoint& /*point*/)
{
}

void Copy(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 1);
    *ipoint.mPoint = *ipoint.mParents[0]->mPoint;
}

void Load(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 1);
    *ipoint.mPoint = *ipoint.mParents[0]->mPoint;
}

void Interpreter::Store(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 2);
    *ipoint.mParents[0]->mPoint = *ipoint.mParents[1]->mPoint;
    *ipoint.mPoint = *ipoint.mParents[1]->mPoint;
    mDirtyStores.push_back(ipoint.mParents[0]);
}

ComputeFunction Tick()
{
    uint64_t tick=0;
    return [tick](InterPoint& ipoint) mutable
    {
        *ipoint.mPoint = ++tick;
    };
}

void Ternary(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 3);
    if(ipoint.mParents[0]->mPoint->mVal)
    {
        *ipoint.mPoint = *ipoint.mParents[1]->mPoint;
    }
    else
    {
        *ipoint.mPoint = *ipoint.mParents[2]->mPoint;
    }
}

#define FUNCTOR(_NAME, _T, _FUNC) \
struct _NAME \
{ \
    _T operator() (_T x) \
    { \
        return _FUNC(x); \
    } \
}; \

#define FUNCTOR_TWO_ARG(_NAME, _T, _FUNC) \
struct _NAME \
{ \
    _T operator() (_T x, _T y) \
    { \
        return _FUNC(x, y); \
    } \
}; \

FUNCTOR_TWO_ARG(MinFunc, double, std::min);
FUNCTOR_TWO_ARG(MaxFunc, double, std::max);
FUNCTOR_TWO_ARG(ModFunc, double, std::fmod);
FUNCTOR(ExpFunc, double, std::exp);
FUNCTOR(LogFunc, double, std::log);
FUNCTOR(TruncFunc, double, std::trunc);

template<typename Op> 
void LoopOperator(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() >= 2);
    Op o;
    auto p = ipoint.mParents.begin();
    auto& point = *ipoint.mPoint;
    point = *(*p)->mPoint;
    for(p++; p != ipoint.mParents.end(); p++)
    {
        point = o(point.mVal, (*p)->mPoint->mVal);
    }
}

template<typename Op> 
void UnaryOperator(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 1);
    Op o;
    *ipoint.mPoint = o(ipoint.mParents[0]->mPoint->mVal);
}

template<typename Op> 
void PairOperator(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 2);
    Op o;
    *ipoint.mPoint = o(ipoint.mParents[0]->mPoint->mVal, ipoint.mParents[1]->mPoint->mVal);
}

void Null(InterPoint&)
{
}

std::function<ComputeFunction ()> Wrap(ComputeFunction func)
{
    return [func]() -> ComputeFunction {return func;};
}

#define WRAP(__FUNC) \
    [this]() -> ComputeFunction {return [this](InterPoint& ipoint) {this->__FUNC(ipoint);};} \

static InterPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",          CountValueValidator<3,3>},   Wrap(Ternary)},
    {{"+",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::plus<double>>)},
    {{"-",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::minus<double>>)},
    {{"/",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::divides<double>>)},
    {{"*",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::multiplies<double>>)},
    {{"%",          MinCountValueValidator<2>},  Wrap(LoopOperator<ModFunc>)},
    {{"<",          CountValueValidator<2,2>},   Wrap(PairOperator<std::less<double>>)},
    {{"<=",         CountValueValidator<2,2>},   Wrap(PairOperator<std::less_equal<double>>)},
    {{">",          CountValueValidator<2,2>},   Wrap(PairOperator<std::greater<double>>)},
    {{">=",         CountValueValidator<2,2>},   Wrap(PairOperator<std::greater_equal<double>>)},
    {{"==",         CountValueValidator<2,2>},   Wrap(PairOperator<std::equal_to<double>>)},
    {{"!=",         CountValueValidator<2,2>},   Wrap(PairOperator<std::not_equal_to<double>>)},
    {{"&&",         MinCountValueValidator<2>},  Wrap(LoopOperator<std::logical_and<double>>)},
    {{"||",         MinCountValueValidator<2>},  Wrap(LoopOperator<std::logical_or<double>>)},
    {{"min",        MinCountValueValidator<2>},  Wrap(LoopOperator<MinFunc>)},
    {{"max",        MinCountValueValidator<2>},  Wrap(LoopOperator<MaxFunc>)},
    {{"exp",        CountValueValidator<1,1>},   Wrap(UnaryOperator<ExpFunc>)},
    {{"ln",         CountValueValidator<1,1>},   Wrap(UnaryOperator<LogFunc>)},
    {{"trunc",      CountValueValidator<1,1>},   Wrap(UnaryOperator<TruncFunc>)},
    {{"not",        CountValueValidator<1,1>},   Wrap(UnaryOperator<std::logical_not<double>>)},
    {{"tick",       MinCountValueValidator<0>},  Tick},
    {{"copy",       MinCountValueValidator<1>},  Wrap(Copy)},
    {{"load",       CountValueValidator<1,1>},   Wrap(Copy)},
    {{"sim-apply",  DummyValidator},  Wrap(Null)}
};

Interpreter::Interpreter()
{ 
    for(auto& jpp : AVAILABLE_PROCS)
    {
        mPointProcessors.push_back(jpp);
    }
    mPointProcessors.push_back({{"store",      CountValueValidator<2,2>},   WRAP(Store)});
}

std::string Interpreter::GetDOTGraph() const
{
    return "digraph " + mGraph->GetDOTGraph();
}

ComputeFunction Interpreter::LookupComputeFunction(Node::Ptr node)
{
    switch(node->mKind)
    {
        case Node::KIND_CONST:
        case Node::KIND_BIND:
        case Node::KIND_VAR:
        case Node::KIND_LIST:
            return ConstDummy;
        default:
        {
            for(auto& proc : mPointProcessors)
            {
                if(node->mToken.compare(proc.procedure.id) == 0)
                {
                    return proc.func();
                }
            }
        }
    }
    assert(false);
    return nullptr;
}

static size_t FindNodeOffset(const std::vector<Node::Ptr>& nodes, Node::Ptr node)
{
    auto niter = std::find(std::begin(nodes), std::end(nodes), node);
    assert(niter != nodes.end());
    return std::distance(nodes.begin(), niter);
}

void Interpreter::CompleteBuild()
{
    const auto nodeLayout = mGraph->GetLayout();

    // For cache niceness
    mInterPointGraph.resize(nodeLayout.size());
    mPoints.resize(nodeLayout.size());

    // Finish adding bulk of logic
    for(const auto& node : nodeLayout)
    {
        size_t offset = FindNodeOffset(nodeLayout, node);
        auto& point = mInterPointGraph[offset];

        point.mHeight = node->mHeight;

        for(const auto& pnode : node->mParents)
        {
            auto& parent = mInterPointGraph[FindNodeOffset(nodeLayout, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }

        point.mComputeFunction = LookupComputeFunction(node);
        point.mPoint = &mPoints[offset];

        if(node->mKind == Node::KIND_CONST)
        {
            *point.mPoint = std::stod(node->mToken);
        }
        else if(node->mKind == Node::KIND_VAR)
        {
            *point.mPoint = node->mInitValue;
        }

        if(node->mInputOffset >= 0)
        {
            for(const auto& label : node->mInputLabels)
            {
                mInputs[label] = point.mPoint;
            }
            point.mPoint->mLength = node->mLength;
        }
        if(node->mObserverOffset >= 0)
        {
            for(const auto& label : node->mObserverLabels)
            {
                mObservers[label] = point.mPoint;
            }
            point.mPoint->mLength = node->mLength;
        }
        mRecomputeHeap.emplace(HeightPtrPair{point.mHeight, &point});
    }

    Stabilize();
}

bool Interpreter::IsDirty() const
{
    for(const auto& namep : mInputs)
    {
        auto& interpoint = *namep.second;
        if(interpoint.IsDirty()) return true;
    }
    return false;
}

void Interpreter::Stabilize(bool force)
{
    for(const auto& namep : mInputs)
    {
        auto& point = *namep.second;
        if(force || point.IsDirty())
        {
            auto& interpoint = mInterPointGraph[&point - &mPoints.front()];
            for(auto* child : interpoint.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
            point.Clean();
        }
    }

    for(const auto ds : mDirtyStores)
    {
        for(auto* child : ds->mChildren)
        {
            mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
        }
    }
    mDirtyStores.clear();
    for(auto& hpp : mRecomputeHeap)
    {
        auto& interpoint = *hpp.point;
        interpoint.mComputeFunction(interpoint);
        if(interpoint.IsDirty())
        {
            for(auto* child : interpoint.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
            interpoint.Clean();
        }
    }
    mRecomputeHeap.clear();
}

bool Interpreter::HasInputPoint(const std::string& label) const
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& Interpreter::LookupInputPoint(const std::string& label)
{
    assert(HasInputPoint(label));
    auto niter = mInputs.find(label);
    return *niter->second;
}

std::vector<std::string> Interpreter::GetInputPointLabels() const
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::vector<std::pair<std::string, double>> Interpreter::DumpInputs() const
{
    std::vector<std::pair<std::string, double>> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(std::make_pair(ip.first, ip.second->mVal));
    }
    return ret;
}

bool Interpreter::HasObserverPoint(const std::string& label) const
{
    auto niter = mObservers.find(label);
    return niter != mObservers.end();
}

Point& Interpreter::LookupObserverPoint(const std::string& label)
{
    assert(HasObserverPoint(label));
    auto niter = mObservers.find(label);
    return *niter->second;
}

std::vector<std::string> Interpreter::GetObserverPointLabels() const
{
    std::vector<std::string> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::vector<std::pair<std::string, double>> Interpreter::DumpObservers() const
{
    std::vector<std::pair<std::string, double>> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(std::make_pair(ip.first, ip.second->mVal));
    }
    return ret;
}

bool Interpreter::SupportSimulation() const
{
    return false;
}

int Interpreter::GetNumSimulationFunctions() const
{
    return 0;
}

void Interpreter::CaptureState()
{
    mCapturedState = mPoints;
}

void Interpreter::ResetState()
{
    mPoints = mCapturedState;
}

bool Interpreter::RunSimulationId(int simId)
{
    return true;
}

std::string Interpreter::GetNumSimulationTarget(int simId) const
{
    return "";
}

std::unique_ptr<Graph> Interpreter::BuildAndLoadGraph()
{
    auto graph = std::unique_ptr<Graph>(new Graph);
    std::vector<Procedure> procedures;
    for(const auto &proc : mPointProcessors) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

void Interpreter::AssignGraph(std::unique_ptr<Graph>& graph)
{
    mGraph.swap(graph);
}

std::unique_ptr<IEngine> Interpreter::Build(const std::string& text)
{
    auto interpreter = std::unique_ptr<Interpreter>(new Interpreter());
    auto graph = interpreter->BuildAndLoadGraph();
    graph->Construct(Parse(text));
    interpreter->AssignGraph(graph);
    interpreter->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(interpreter));
}

}

