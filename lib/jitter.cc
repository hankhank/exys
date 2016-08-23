
#include "jitter.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>

namespace Exys
{

Jitter::Jitter(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

std::string Jitter::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
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

void Jitter::CompleteBuild()
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

    // Second pass - set heights and add parents/children and collect inputs and observers
    for(auto node : necessaryNodes)
    {
        //size_t offset = FindNodeOffset(necessaryNodes, node);
        //auto& point = mInterPointGraph[offset];

        //point.mHeight = node->mHeight;

        //for(auto pnode : node->mParents)
        //{
        //    auto& parent = mInterPointGraph[FindNodeOffset(necessaryNodes, pnode)];
        //    point.mParents.push_back(&parent);
        //    parent.mChildren.push_back(&point);
        //}

        std::unordered_map<Node::Ptr, std::string>::iterator ob;
        if(node->mKind == Node::KIND_CONST)
        {
            //point = std::stod(node->mToken);
        }
        else if(node->mKind == Node::KIND_INPUT)
        {
            //mInputs[node->mToken] = &point;
        }
        if((ob = observers.find(node)) != observers.end())
        {
            //mObservers[ob->second] = &point;
        }
    }
    Stabilize();
}

bool Jitter::IsDirty()
{
    return false;
}

void Jitter::Stabilize()
{
}

void Jitter::PointChanged(Point& point)
{
}

bool Jitter::HasInputPoint(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& Jitter::LookupInputPoint(const std::string& label)
{
    //assert(HasInputPoint(label));
    //auto niter = mInputs.find(label);
    //return niter->second->mPoint;
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
        //ret[ip.first] = ip.second->mPoint.mD;
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
    //assert(HasObserverPoint(label));
    //auto niter = mObservers.find(label);
    //return niter->second->mPoint;
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
        //ret[ip.first] = ip.second->mPoint.mD;
    }
    return ret;
}

std::unique_ptr<Graph> BuildAndLoadGraph()
{
    auto graph = std::make_unique<Graph>();
    std::vector<Procedure> procedures;
    //for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

std::unique_ptr<IEngine> Jitter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Build(Parse(text));
    auto engine = std::make_unique<Jitter>(std::move(graph));
    engine->CompleteBuild();
    return engine;
}

}

