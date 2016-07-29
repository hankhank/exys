
#include "exys.h"
#include <iostream>
#include <sstream>
#include <cassert>

namespace Exys
{

Exys::Exys(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

std::string Exys::GetDOTGraph()
{
    return mGraph->GetDOTGraph();
}

void Exys::TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes)
{
    if (height < node->mHeight) node->mHeight = height;
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

    // Collect necessary nodes - nodes that are inputs to an observable
    // node. Also set the heights from observability
    std::set<Node::Ptr> necessaryNodes;
    for(auto ob : mGraph->GetObservers())
    {
        uint64_t height=0;
        TraverseNodes(ob.second, height, necessaryNodes);
    }

    // For cache niceness
    mPointGraph.resize(necessaryNodes.size());
    
    // Second pass - set heights and add parents and collect inputs and observers
    for(auto node : necessaryNodes)
    {
        size_t offset = FindNodeOffset(necessaryNodes, node);
        auto& point = mPointGraph[offset];

        node->mHeight = point.mHeight;

        for(auto pnode : node->mParents)
        {
            auto& parent = mPointGraph[FindNodeOffset(necessaryNodes, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }

        if(node->mKind == Node::KIND_INPUT)    mInputs[node->mToken] = &point;
        if(node->mKind == Node::KIND_OBSERVER) mObservers[node->mToken] = &point;
    }
}

//Node::Ptr Exys::LookupInputNode(const std::string& label)
//{
//    auto niter = mInputs.find(label);
//    return niter != mInputs.end() ? niter->second : nullptr;
//}
//
//Node::Ptr Exys::LookupObserverNode(const std::string& label)
//{
//    auto niter = mObservers.find(label);
//    return niter != mObservers.end() ? niter->second : nullptr;
//}

std::unique_ptr<Graph> BuildAndLoadGraph()
{
    std::vector<std::string> procs;
    procs.push_back("+");
    procs.push_back("-");
    procs.push_back("/");
    procs.push_back("*");
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

