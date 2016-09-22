#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <set>

#include "exys.h"

namespace Exys
{

struct InterPoint;

typedef std::function<void (InterPoint&)> ComputeFunction;

struct InterPoint : Point
{
    int64_t mHeight;
    std::vector<InterPoint*> mParents;
    std::vector<InterPoint*> mChildren;
    ComputeFunction mComputeFunction;

    Point& operator[](size_t i) override
    {
        return *(this+i);
    }

    bool operator!=(const InterPoint& rhs) { return mVal != rhs.mVal; }

    InterPoint& operator=(InterPoint ip) {mDirty = (mVal != ip.mVal); mVal = ip.mVal; return *this;}
    InterPoint& operator=(double d)      {mDirty = (mVal != d); mVal = d; return *this;}
};

class Interpreter : public IEngine
{
public:
    Interpreter(std::unique_ptr<Graph> graph);

    virtual ~Interpreter() {}

    void Stabilize() override;
    bool IsDirty() override;

    bool HasInputPoint(const std::string& label) override;
    Point& LookupInputPoint(const std::string& label) override;
    std::vector<std::string> GetInputPointLabels() override;
    std::unordered_map<std::string, double> DumpInputs() override;

    bool HasObserverPoint(const std::string& label) override;
    Point& LookupObserverPoint(const std::string& label) override;
    std::vector<std::string> GetObserverPointLabels() override;
    std::unordered_map<std::string, double> DumpObservers() override;

    std::string GetDOTGraph() override;

    static std::unique_ptr<IEngine> Build(const std::string& text);

private:
    void CompleteBuild();
    void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes);
    void CollectListMembers(Node::Ptr node, std::vector<Node::Ptr>& nodes);
    ComputeFunction LookupComputeFunction(Node::Ptr node);
    
    std::unordered_map<std::string, InterPoint*> mObservers;
    std::unordered_map<std::string, InterPoint*> mInputs;

    std::vector<InterPoint> mInterPointGraph;

    struct HeightPtrPair
    {
        int64_t height;
        InterPoint* point;

        bool operator<(const HeightPtrPair& rhs) const
        {
            return ((height > rhs.height) || ((height == rhs.height) && (point > rhs.point)));
        }
    };
    std::set<HeightPtrPair> mRecomputeHeap; // height -> Nodes
    std::unique_ptr<Graph> mGraph;
};

};
