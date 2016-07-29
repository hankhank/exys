#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <set>

#include "parser.h"
#include "graph.h"

namespace Exys
{

union Signal
{
    bool b;
    int i;
    unsigned int u;
    double d;
};

struct Point
{
    typedef std::shared_ptr<Node> Ptr;

    Signal mSignal;
    uint64_t mHeight;
    uint64_t mChangeId;
    uint64_t mRecomputeId;
    std::vector<Point*> mParents;
    std::vector<Point*> mChildren;
    std::function<void (Point*)> mComputeFunction;
};

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
    void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes);
    
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::vector<Point> mPointGraph;

    std::map<uint64_t, Point::Ptr> recomputeHeap; // height -> Nodes
    uint64_t stabilisationId=1;

    std::unique_ptr<Graph> mGraph;
};

};
