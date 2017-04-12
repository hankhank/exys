
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

struct InterPointProcessor
{
    Procedure procedure;
    std::function<ComputeFunction ()> func;
};

void ConstDummy(InterPoint& /*point*/)
{
}

void Copy(InterPoint& ipoint)
{
    assert(ipoint.mParents.size() == 1);
    *ipoint.mPoint = *ipoint.mParents[0]->mPoint;
}

ComputeFunction Latch()
{
    Point lastPoint;
    return [lastPoint](InterPoint& ipoint) mutable
    {
        assert(ipoint.mParents.size() == 2);
        if (ipoint.mParents[0]->mPoint->mVal)
        {
            lastPoint = *ipoint.mParents[1]->mPoint;
            *ipoint.mPoint = lastPoint;
        }
    };
}

ComputeFunction FlipFlop()
{
    Point lastPoint;
    return [lastPoint](InterPoint& ipoint) mutable
    {
        assert(ipoint.mParents.size() == 2);
        if (ipoint.mParents[0]->mPoint->mVal)
        {
            *ipoint.mPoint = lastPoint;
            lastPoint = *ipoint.mParents[1]->mPoint;
        }
    };
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
FUNCTOR(ExpFunc, double, std::exp);
FUNCTOR(LogFunc, double, std::log);

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

static InterPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",          CountValueValidator<3,3>},   Wrap(Ternary)},
    {{"+",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::plus<double>>)},
    {{"-",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::minus<double>>)},
    {{"/",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::divides<double>>)},
    {{"*",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::multiplies<double>>)},
    //{{"%",          DummyValidator},Wrap(  LoopOperator<std::modulus<double>>)},
    {{"<",          CountValueValidator<2,2>},   Wrap(PairOperator<std::less<double>>)},
    {{"<=",         CountValueValidator<2,2>},   Wrap(PairOperator<std::less_equal<double>>)},
    {{">",          CountValueValidator<2,2>},   Wrap(PairOperator<std::greater<double>>)},
    {{">=",         CountValueValidator<2,2>},   Wrap(PairOperator<std::greater_equal<double>>)},
    {{"==",         CountValueValidator<2,2>},   Wrap(PairOperator<std::equal_to<double>>)},
    {{"!=",         CountValueValidator<2,2>},   Wrap(PairOperator<std::not_equal_to<double>>)},
    {{"&&",         CountValueValidator<2,2>},   Wrap(PairOperator<std::logical_and<double>>)},
    {{"||",         CountValueValidator<2,2>},   Wrap(PairOperator<std::logical_or<double>>)},
    {{"min",        MinCountValueValidator<2>},  Wrap(LoopOperator<MinFunc>)},
    {{"max",        MinCountValueValidator<2>},  Wrap(LoopOperator<MaxFunc>)},
    {{"exp",        CountValueValidator<1,1>},   Wrap(UnaryOperator<ExpFunc>)},
    {{"ln",         CountValueValidator<1,1>},   Wrap(UnaryOperator<LogFunc>)},
    {{"not",        CountValueValidator<1,1>},   Wrap(UnaryOperator<std::logical_not<double>>)},
    {{"latch",      CountValueValidator<2,2>},   Latch},
    {{"flip-flop",  CountValueValidator<2,2>},   FlipFlop},
    {{"tick",       MinCountValueValidator<0>},  Tick},
    {{"copy",       MinCountValueValidator<1>},  Wrap(Copy)},
    {{"sim-apply",  MinCountValueValidator<2>},  Wrap(Null)}
};

Interpreter::Interpreter(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

std::string Interpreter::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
}

ComputeFunction Interpreter::LookupComputeFunction(Node::Ptr node)
{
    switch(node->mKind)
    {
        case Node::KIND_CONST:
        case Node::KIND_VAR:
        case Node::KIND_LIST:
            return ConstDummy;
        default:
        {
            for(auto& proc : AVAILABLE_PROCS)
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
    auto nodeLayout = mGraph->GetLayout();

    // For cache niceness
    mInterPointGraph.resize(nodeLayout.size());
    mPoints.resize(nodeLayout.size());

    // Finish adding bulk of logic
    for(auto node : nodeLayout)
    {
        size_t offset = FindNodeOffset(nodeLayout, node);
        auto& point = mInterPointGraph[offset];

        point.mHeight = node->mHeight;

        for(auto pnode : node->mParents)
        {
            auto& parent = mInterPointGraph[FindNodeOffset(nodeLayout, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }

        point.mComputeFunction = LookupComputeFunction(node);
        point.mPoint = &mPoints[offset];

        std::unordered_map<Node::Ptr, std::string>::iterator ob;
        if(node->mKind == Node::KIND_CONST)
        {
            *point.mPoint = std::stod(node->mToken);
        }
        if(node->mIsInput)
        {
            for(const auto& label : node->mInputLabels)
            {
                mInputs[label] = point.mPoint;
            }
            point.mPoint->mLength = node->mLength;
        }
        if(node->mIsObserver)
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

bool Interpreter::IsDirty()
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

bool Interpreter::HasInputPoint(const std::string& label)
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

std::vector<std::string> Interpreter::GetInputPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Interpreter::DumpInputs()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mInputs)
    {
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

bool Interpreter::HasObserverPoint(const std::string& label)
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

std::vector<std::string> Interpreter::GetObserverPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Interpreter::DumpObservers()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mObservers)
    {
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

int Interpreter::GetNumSimulationFunctions() 
{
    assert(false && "Not implemented");
}

void Interpreter::CaptureState()
{
    assert(false && "Not implemented");
}

void Interpreter::ResetState()
{
    assert(false && "Not implemented");
}

bool Interpreter::RunSimulationId(int simId)
{
    assert(false && "Not implemented");
}

static std::unique_ptr<Graph> BuildAndLoadGraph()
{
    auto graph = std::unique_ptr<Graph>(new Graph);
    std::vector<Procedure> procedures;
    for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

std::unique_ptr<IEngine> Interpreter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Construct(Parse(text));
    auto engine = std::unique_ptr<Interpreter>(new Interpreter(std::move(graph)));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

