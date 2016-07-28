#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>

#include "parser.h"
#include "graph.h"

namespace Exys
{

//union Signal
//{
//    bool b;
//    int i;
//    unsigned int u;
//    double d;
//};
//
//template
//void sum(Point point)
//{
//}
//
//struct Point
//{
//
//};
//
class Exys
{
public:
    Exys(std::unique_ptr<Graph> graph);

    void Stabilize(uint64_t stabilisationId);

    //Point::Ptr LookupInputPoint(const std::string& label);
    //Point::Ptr LookupObserverPoint(const std::string& label);

    std::string GetDOTGraph();

    static std::unique_ptr<Exys> Build(const std::string& text);

private:
    void CompleteBuild();
    //void RecursiveHeightSet(Node::Ptr node, uint64_t& height);
    
    std::vector<Node::Ptr> mAllNodes;
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::unordered_map<std::string, Node::Ptr> mObservers;
    std::unordered_map<std::string, Node::Ptr> mInputs;

    std::map<uint64_t, Node::Ptr> recomputeHeap; // height -> Nodes
    uint64_t stabilisationId=1;

    std::unique_ptr<Graph> mGraph;
};

};
