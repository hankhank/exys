#pragma once

#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <set>

#include "exys.h"

namespace Exys
{

struct InterPoint;

typedef std::function<void (InterPoint&)> ComputeFunction;

struct InterPoint
{
    int64_t mHeight;
    std::vector<InterPoint*> mParents;
    std::vector<InterPoint*> mChildren;
    Point* mPoint;
    ComputeFunction mComputeFunction;

    bool IsDirty() const { return mPoint->mDirty; };
    void Clean() { mPoint->mDirty = false; };
};

class Interpreter : public IEngine
{
public:
    Interpreter(std::unique_ptr<Graph> graph);

    virtual ~Interpreter() {}

    void Stabilize(bool force=false) override;
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
    
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::vector<InterPoint> mInterPointGraph;
    std::vector<Point> mPoints;

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
