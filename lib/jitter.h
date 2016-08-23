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

class Jitter : public IEngine
{
public:
    Jitter(std::unique_ptr<Graph> graph);

    virtual ~Jitter() {}

    void PointChanged(Point& point) override;
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

    std::unordered_map<std::string, void*> mObservers;
    std::unordered_map<std::string, void*> mInputs;

    uint64_t mStabilisationId=1;

    std::unique_ptr<Graph> mGraph;
};

};
