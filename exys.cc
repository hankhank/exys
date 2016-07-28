
#include "exys.h"
#include <set>
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

//void Exys::RecursiveHeightSet(Node::Ptr node, uint64_t& height)
//{
//    if (height < node->mHeight) node->mHeight = height;
//    node->mNecessary = true;
//    height++;
//    for(auto parent : node->mParents)
//    {
//        RecursiveHeightSet(parent, height);
//    }
//}

void Exys::CompleteBuild()
{
    // Set heights add children to necessary nodes
    //for(auto ob : mObservers)
    //{
    //    uint64_t height = 0;
    //    RecursiveHeightSet(ob.second, height);
    //}
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

