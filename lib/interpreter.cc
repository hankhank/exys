
#include "exys.h"
#include <iostream>
#include <sstream>
#include <cassert>

namespace Exys
{

struct PointProcessor
{
    Procedure procedure;
    ComputeFunction func; 
};

void ConstDummy(Point& /*point*/)
{
}

//template<typename T, template<typename T> typename Op>
//const T& OverloadWrap(const T& a, const T& b)
//{
//    //Op<T> o;
//    return Op<T>(a, b);
//}

void Ternary(Point& point)
{
    assert(point.mParents.size() == 3);
    if(point.mParents[0]->mSignal.d)
    {
        point = point.mParents[1]->mSignal.d;
    }
    else
    {
        point = point.mParents[2]->mSignal.d;
    }
}

template<typename Op> 
void LoopOperator(Point& point)
{
    assert(point.mParents.size() >= 2);
    Op o;
    auto p = point.mParents.begin();
    point = (*p)->mSignal.d;
    for(p++; p != point.mParents.end(); p++)
    {
        point = o(point.mSignal.d, (*p)->mSignal.d);
    }
}

template<typename Op> 
void UnaryOperator(Point& point)
{
    assert(point.mParents.size() == 1);
    Op o;
    point = o(point.mParents[0]->mSignal.d);
}

template<typename Op> 
void PairOperator(Point& point)
{
    assert(point.mParents.size() == 2);
    Op o;
    point = o(point.mParents[0]->mSignal.d, point.mParents[1]->mSignal.d);
}

void MulDouble(Point& point)
{
}

void DummyValidator(Node::Ptr)
{
}

PointProcessor AVAILABLE_PROCS[] =
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
    //{{"min",  DummyValidator},  LoopOperator<OverloadWrap<double, std::min>>},
    //{{"max",  DummyValidator},  LoopOperator<OverloadWrap<double, std::max>>},
    //{{"exp",  DummyValidator},  MulDouble},
    //{{"ln",   DummyValidator},  MulDouble},
    {{"not",  DummyValidator},  UnaryOperator<std::logical_not<double>>}
};

Exys::Exys(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

std::string Exys::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
}

ComputeFunction Exys::LookupComputeFunction(Node::Ptr node)
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

void Exys::TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes)
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

void Exys::CompleteBuild()
{
    // First pass - Type checking and adding in casts
    // TODO

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

    // For cache niceness
    mPointGraph.resize(necessaryNodes.size());
    
    // Second pass - set heights and add parents/children and collect inputs and observers
    for(auto node : necessaryNodes)
    {
        size_t offset = FindNodeOffset(necessaryNodes, node);
        auto& point = mPointGraph[offset];

        point.mHeight = node->mHeight;

        for(auto pnode : node->mParents)
        {
            auto& parent = mPointGraph[FindNodeOffset(necessaryNodes, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }

        point.mComputeFunction = LookupComputeFunction(node);

        std::unordered_map<Node::Ptr, std::string>::iterator ob;
        if(node->mKind == Node::KIND_CONST)
        {
            point = std::stod(node->mToken);
        }
        else if(node->mKind == Node::KIND_INPUT)
        {
            mInputs[node->mToken] = &point;
        }
        if((ob = observers.find(node)) != observers.end())
        {
            mObservers[ob->second] = &point;
        }

        mRecomputeHeap.emplace(HeightPtrPair{point.mHeight, &point});
    }
    Stabilize();
}

bool Exys::IsDirty()
{
    return !mRecomputeHeap.empty();
}

void Exys::Stabilize()
{
    for(auto& hpp : mRecomputeHeap)
    {
        auto* point = hpp.point;

        Signal old = point->mSignal;
        point->mComputeFunction(*point);

        if(old != point->mSignal)
        {
            point->mChangeId = mStabilisationId;
            for(auto* child : point->mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
        }
        point->mRecomputeId = mStabilisationId;
    }
    mStabilisationId++;
    mRecomputeHeap.clear();
}

void Exys::PointChanged(Point& point)
{
    for(auto* child : point.mChildren)
    {
        mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
    }
}

bool Exys::HasInputPoint(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& Exys::LookupInputPoint(const std::string& label)
{
    assert(HasInputPoint(label));
    auto niter = mInputs.find(label);
    return *niter->second;
}

std::vector<std::string> Exys::GetInputPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Exys::DumpInputs()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mInputs)
    {
        ret[ip.first] = ip.second->mSignal.d;
    }
    return ret;
}

bool Exys::HasObserverPoint(const std::string& label)
{
    auto niter = mObservers.find(label);
    return niter != mObservers.end();
}

Point& Exys::LookupObserverPoint(const std::string& label)
{
    assert(HasObserverPoint(label));
    auto niter = mObservers.find(label);
    return *niter->second;
}

std::vector<std::string> Exys::GetObserverPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Exys::DumpObservers()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mObservers)
    {
        ret[ip.first] = ip.second->mSignal.d;
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

std::unique_ptr<Exys> Exys::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Build(Parse(text));
    auto exys = std::make_unique<Exys>(std::move(graph));
    exys->CompleteBuild();
    return exys;
}

}

