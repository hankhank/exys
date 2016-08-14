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

union Signal
{
    bool b;
    int i;
    unsigned int u;
    double d;
    bool operator!=(const Signal& rhs)
    {
        return d != rhs.d;
    }
};

struct Point;

typedef std::function<void (Point&)> ComputeFunction;

struct Point
{
    typedef std::shared_ptr<Node> Ptr;

    Signal mSignal;
    uint64_t mHeight;
    uint64_t mChangeId;
    uint64_t mRecomputeId;
    std::vector<Point*> mParents;
    std::vector<Point*> mChildren;
    ComputeFunction mComputeFunction;

    Point& operator=(bool b) {mSignal.b = b;}
    Point& operator=(int i)  {mSignal.i = i;}
    Point& operator=(unsigned int u) {mSignal.u = u;}
    Point& operator=(double d) {mSignal.d = d;}

    bool operator==(bool b) {return mSignal.b = b;}
    bool operator==(int i)  {return mSignal.i = i;}
    bool operator==(unsigned int u) {return mSignal.u = u;}
    bool operator==(double d) {return mSignal.d = d;}

    bool operator!=(bool b) {return mSignal.b != b;}
    bool operator!=(int i)  {return mSignal.i != i;}
    bool operator!=(unsigned int u) {return mSignal.u != u;}
    bool operator!=(double d) {return mSignal.d != d;}
};

class Exys
{
public:
    Exys(std::unique_ptr<Graph> graph);

    void PointChanged(Point& point);
    void Stabilize();
    bool IsDirty();

    bool HasInputPoint(const std::string& label);
    Point& LookupInputPoint(const std::string& label);
    std::vector<std::string> GetInputPointLabels();

    bool HasObserverPoint(const std::string& label);
    Point& LookupObserverPoint(const std::string& label);
    std::vector<std::string> GetObserverPointLabels();

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
