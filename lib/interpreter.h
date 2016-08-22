#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <set>

#include "graph.h"

namespace Exys
{

struct InterPoint;

typedef std::function<void (InterPoint&)> ComputeFunction;

struct InterPoint
{
    Point mPoint;
    uint64_t mHeight;
    uint64_t mChangeId;
    uint64_t mRecomputeId;
    std::vector<InterPoint*> mParents;
    std::vector<InterPoint*> mChildren;
    ComputeFunction mComputeFunction;
};

class Interpreter : public IEngine
{
public:
    Exys(std::unique_ptr<Graph> graph);

    void PointChanged(Point& point);
    void Stabilize();
    bool IsDirty();

    bool HasInputPoint(const std::string& label);
    Point& LookupInputPoint(const std::string& label);
    std::vector<std::string> GetInputPointLabels();
    std::unordered_map<std::string, double> DumpInputs();

    bool HasObserverPoint(const std::string& label);
    Point& LookupObserverPoint(const std::string& label);
    std::vector<std::string> GetObserverPointLabels();
    std::unordered_map<std::string, double> DumpObservers();

    std::string GetDOTGraph();

    static std::unique_ptr<Exys> Build(const std::string& text);

private:
    void CompleteBuild();
    void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes);
    ComputeFunction LookupComputeFunction(Node::Ptr node);
    
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::vector<Point> mPointGraph;

    struct HeightPtrPair
    {
        uint64_t height;
        Point* point;

        bool operator<(const HeightPtrPair& rhs) const
        {
            return (height > rhs.height) || ((height == rhs.height) && (point > rhs.point));
        }
    };
    std::set<HeightPtrPair> mRecomputeHeap; // height -> Nodes
    uint64_t mStabilisationId=1;

    std::unique_ptr<Graph> mGraph;
};

};
