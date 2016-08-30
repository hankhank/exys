
#include "interpreter.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>

namespace Exys
{

struct InterPointProcessor
{
    Procedure procedure;
    ComputeFunction func; 
};

void ConstDummy(InterPoint& /*point*/)
{
}

void Ternary(InterPoint& point)
{
    assert(point.mParents.size() == 3);
    if(point.mParents[0]->mPoint.mD)
    {
        point = *point.mParents[1];
    }
    else
    {
        point = *point.mParents[2];
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
void LoopOperator(InterPoint& point)
{
    assert(point.mParents.size() >= 2);
    Op o;
    auto p = point.mParents.begin();
    point = (*p)->mPoint.mD;
    for(p++; p != point.mParents.end(); p++)
    {
        point = o(point.mPoint.mD, (*p)->mPoint.mD);
    }
}

template<typename Op> 
void UnaryOperator(InterPoint& point)
{
    assert(point.mParents.size() == 1);
    Op o;
    point = o(point.mParents[0]->mPoint.mD);
}

template<typename Op> 
void PairOperator(InterPoint& point)
{
    assert(point.mParents.size() == 2);
    Op o;
    point = o(point.mParents[0]->mPoint.mD, point.mParents[1]->mPoint.mD);
}

void MulDouble(InterPoint& point)
{
}

void DummyValidator(Node::Ptr)
{
}

InterPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",    DummyValidator},  Ternary},
    {{"+",    DummyValidator},  LoopOperator<std::plus<double>>},
    {{"-",    DummyValidator},  LoopOperator<std::minus<double>>},
    {{"/",    DummyValidator},  LoopOperator<std::divides<double>>},
    {{"*",    DummyValidator},  LoopOperator<std::multiplies<double>>},
    //{{"%",    DummyValidator},  LoopOperator<std::modulus<double>>},
    {{"<",    DummyValidator},  PairOperator<std::less<double>>},
    {{"<=",   DummyValidator},  PairOperator<std::less_equal<double>>},
    {{">",    DummyValidator},  PairOperator<std::greater<double>>},
    {{">=",   DummyValidator},  PairOperator<std::greater_equal<double>>},
    {{"==",   DummyValidator},  PairOperator<std::equal_to<double>>},
    {{"!=",   DummyValidator},  PairOperator<std::not_equal_to<double>>},
    {{"&&",   DummyValidator},  PairOperator<std::logical_and<double>>},
    {{"||",   DummyValidator},  PairOperator<std::logical_or<double>>},
    {{"min",  DummyValidator},  LoopOperator<MinFunc>},
    {{"max",  DummyValidator},  LoopOperator<MaxFunc>},
    {{"exp",  DummyValidator},  UnaryOperator<ExpFunc>},
    {{"ln",   DummyValidator},  UnaryOperator<LogFunc>},
    {{"not",  DummyValidator},  UnaryOperator<std::logical_not<double>>}
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

void Interpreter::TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes)
{
    if (height > node->mHeight) node->mHeight = height;
    height++;

    necessaryNodes.insert(node);
    for(auto parent : node->mParents)
    {
        TraverseNodes(parent, height, necessaryNodes);
    }
}

size_t FindNodeOffset(const std::set<Node::Ptr>& nodes, Node::Ptr node)
{
    return std::distance(nodes.begin(), nodes.find(node));
}

void Interpreter::CompleteBuild()
{
    // First pass - Type checking and adding in casts
    // TODO

    // Collect necessary nodes - nodes that are inputs to an observable
    // node. Also set the heights from observability
    std::set<Node::Ptr> necessaryNodes;
    for(auto ob : mGraph->GetObservers())
    {
        uint64_t height=0;
        TraverseNodes(ob.second, height, necessaryNodes);
    }

    // For cache niceness
    mInterPointGraph.resize(necessaryNodes.size());
    
    // Second pass - set heights and add parents/children and collect inputs and observers
    for(auto node : necessaryNodes)
    {
        size_t offset = FindNodeOffset(necessaryNodes, node);
        auto& point = mInterPointGraph[offset];

        point.mHeight = node->mHeight;

        for(auto pnode : node->mParents)
        {
            auto& parent = mInterPointGraph[FindNodeOffset(necessaryNodes, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }

        point.mComputeFunction = LookupComputeFunction(node);

        if(node->mKind == Node::KIND_CONST)
        {
            point = std::stod(node->mToken);
        }
        else if(node->mKind == Node::KIND_INPUT)
        {
            mInputs[node->mToken] = &point;
        }
        if(node->mIsObserver)
        {
            mObservers[node->mToken] = &point;
        }

        mRecomputeHeap.emplace(HeightPtrPair{point.mHeight, &point});
    }
    Stabilize();
}

bool Interpreter::IsDirty()
{
    return !mRecomputeHeap.empty();
}

void Interpreter::Stabilize()
{
    for(auto& hpp : mRecomputeHeap)
    {
        auto& point = *hpp.point;

        InterPoint old = point;
        point.mComputeFunction(point);

        if(old != point)
        {
            point.mChangeId = mStabilisationId;
            for(auto* child : point.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
        }
        point.mRecomputeId = mStabilisationId;
    }
    mStabilisationId++;
    mRecomputeHeap.clear();
}

void Interpreter::PointChanged(Point& point)
{
    auto* interpoint = (InterPoint*) &point; // Ugly c hack 
    for(auto* child : interpoint->mChildren)
    {
        mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
    }
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
    return niter->second->mPoint;
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
        ret[ip.first] = ip.second->mPoint.mD;
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
    return niter->second->mPoint;
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
        ret[ip.first] = ip.second->mPoint.mD;
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

std::unique_ptr<IEngine> Interpreter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Build(Parse(text));
    auto engine = std::make_unique<Interpreter>(std::move(graph));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

