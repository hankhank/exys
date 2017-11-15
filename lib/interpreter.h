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

struct InterPointProcessor
{
    Procedure procedure;
    std::function<ComputeFunction ()> func;
};

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
    Interpreter();

    virtual ~Interpreter() {}

    void Stabilize(bool force=false) override;
    bool IsDirty() const override;

    bool HasInputPoint(const std::string& label) const override;
    Point& LookupInputPoint(const std::string& label) override;
    std::vector<std::string> GetInputPointLabels() const override;
    std::vector<std::pair<std::string, double>> DumpInputs() const override;

    bool HasObserverPoint(const std::string& label) const override;
    Point& LookupObserverPoint(const std::string& label) override;
    std::vector<std::string> GetObserverPointLabels() const override;
    std::vector<std::pair<std::string, double>> DumpObservers() const override;

    bool SupportSimulation() const override;
    int GetNumSimulationFunctions() const override;
    void CaptureState() override;
    void ResetState() override;
    bool RunSimulationId(int simId) override;
    std::string GetNumSimulationTarget(int simId) const override;

    std::string GetDOTGraph() const override;

    static std::unique_ptr<IEngine> Build(const std::string& text);

private:
    void AssignGraph(std::unique_ptr<Graph>& graph);
    void CompleteBuild();
    void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes);
    void CollectListMembers(Node::Ptr node, std::vector<Node::Ptr>& nodes);
    ComputeFunction LookupComputeFunction(Node::Ptr node);
    std::unique_ptr<Graph> BuildAndLoadGraph();

    void Store(InterPoint& ipoint);
    
    std::unordered_map<std::string, Point*> mObservers;
    std::unordered_map<std::string, Point*> mInputs;

    std::vector<InterPoint> mInterPointGraph;
    std::vector<Point> mPoints;
    std::vector<InterPoint*> mDirtyStores;

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
    std::vector<InterPointProcessor> mPointProcessors;
};

};
