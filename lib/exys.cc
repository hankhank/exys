
#include "exys.h"
#include <iostream>
#include <sstream>
#include <cassert>

namespace Exys
{

struct Procs
{
    const char* id;
    ComputeFunction func; 
};

void ConstDummy(Point& /*point*/)
{
}

void Ternary(Point& point)
{
}

void SumDouble(Point& point)
{
    point = 0.0;
    for(auto* parent : point.mParents)
    {
        point = (double)point.mSignal.d + parent->mSignal.d;
    }
}

void SubDouble(Point& point){}
void DivDouble(Point& point){}
void MulDouble(Point& point){}

Procs AVAILABLE_PROCS[] =
{
    {"?", Ternary},
    {"+", SumDouble},
    {"-", SubDouble},
    {"/", DivDouble},
    {"*", MulDouble},
    {"<", MulDouble},
    {"<=", MulDouble},
    {">", MulDouble},
    {">=", MulDouble},
    {"==", MulDouble},
    {"!=", MulDouble},
    {"min", MulDouble},
    {"max", MulDouble},
    {"exp", MulDouble},
    {"ln", MulDouble},
    {"not", MulDouble}
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
        if(node->mToken.compare(proc.id) == 0)
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
        else if((ob = observers.find(node)) != observers.end())
        {
            mObservers[ob->second] = &point;
        }
    }
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

std::unique_ptr<Graph> BuildAndLoadGraph()
{
    std::vector<std::string> procs;
    for(auto& proc : AVAILABLE_PROCS)
    {
        procs.push_back(proc.id);
    }
    auto graph = std::make_unique<Graph>();
    graph->SetSupportedProcedures(procs);
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

