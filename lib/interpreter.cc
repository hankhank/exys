
#include "interpreter.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <cmath>

// Notes
// Handling list inputs and outputs is a bit of pain and not exactly symmetrical
// Concerns
// 1. To allow array like access we need to have all inputs and outputs
//    next to one another not randomly placed through out the graph
// 2. We need to add in extra mappings to the start of the arrays along from actual
//    array member usage
// 3. Observables need an extra copy incase list members exist in two output arrays.
//    We could handle the two cases avoiding the extra copy but its annoying.
// 4. We can't pass back Point to the user because they need special [] operators
//    to take into account the special interpoint type we use internally

namespace Exys
{

struct InterPointProcessor
{
    Procedure procedure;
    ComputeFunction func; 
};

void ConstDummy(InterPoint& /*point*/)
{
}

void Copy(InterPoint& point)
{
    assert(point.mParents.size() == 1);
    point = *point.mParents[0];
}

void Ternary(InterPoint& point)
{
    assert(point.mParents.size() == 3);
    if(point.mParents[0]->mVal)
    {
        point = *point.mParents[1];
    }
    else
    {
        point = *point.mParents[2];
    }
}

#define FUNCTOR(_NAME, _T, _FUNC) \
struct _NAME \
{ \
    _T operator() (_T x) \
    { \
        return _FUNC(x); \
    } \
}; \

#define FUNCTOR_TWO_ARG(_NAME, _T, _FUNC) \
struct _NAME \
{ \
    _T operator() (_T x, _T y) \
    { \
        return _FUNC(x, y); \
    } \
}; \

FUNCTOR_TWO_ARG(MinFunc, double, std::min);
FUNCTOR_TWO_ARG(MaxFunc, double, std::max);
FUNCTOR(ExpFunc, double, std::exp);
FUNCTOR(LogFunc, double, std::log);

template<typename Op> 
void LoopOperator(InterPoint& point)
{
    assert(point.mParents.size() >= 2);
    Op o;
    auto p = point.mParents.begin();
    point = (*p)->mVal;
    for(p++; p != point.mParents.end(); p++)
    {
        point = o(point.mVal, (*p)->mVal);
    }
}

template<typename Op> 
void UnaryOperator(InterPoint& point)
{
    assert(point.mParents.size() == 1);
    Op o;
    point = o(point.mParents[0]->mVal);
}

template<typename Op> 
void PairOperator(InterPoint& point)
{
    assert(point.mParents.size() == 2);
    Op o;
    point = o(point.mParents[0]->mVal, point.mParents[1]->mVal);
}

static void DummyValidator(Node::Ptr)
{
}

static InterPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",    DummyValidator},  Ternary},
    {{"+",    DummyValidator},  LoopOperator<std::plus<double>>},
    {{"-",    DummyValidator},  LoopOperator<std::minus<double>>},
    {{"/",    DummyValidator},  LoopOperator<std::divides<double>>},
    {{"*",    DummyValidator},  LoopOperator<std::multiplies<double>>},
    //{{"%",    DummyValidator},  LoopOperator<std::modulus<double>>},
    {{"<",    DummyValidator},  PairOperator<std::less<double>>},
    {{"<=",   DummyValidator},  PairOperator<std::less_equal<double>>},
    {{">",    DummyValidator},  PairOperator<std::greater<double>>},
    {{">=",   DummyValidator},  PairOperator<std::greater_equal<double>>},
    {{"==",   DummyValidator},  PairOperator<std::equal_to<double>>},
    {{"!=",   DummyValidator},  PairOperator<std::not_equal_to<double>>},
    {{"&&",   DummyValidator},  PairOperator<std::logical_and<double>>},
    {{"||",   DummyValidator},  PairOperator<std::logical_or<double>>},
    {{"min",  DummyValidator},  LoopOperator<MinFunc>},
    {{"max",  DummyValidator},  LoopOperator<MaxFunc>},
    {{"exp",  DummyValidator},  UnaryOperator<ExpFunc>},
    {{"ln",   DummyValidator},  UnaryOperator<LogFunc>},
    {{"not",  DummyValidator},  UnaryOperator<std::logical_not<double>>}
};

Interpreter::Interpreter(std::unique_ptr<Graph> graph)
:mGraph(std::move(graph))
{
}

std::string Interpreter::GetDOTGraph()
{
    return "digraph " + mGraph->GetDOTGraph();
}

ComputeFunction Interpreter::LookupComputeFunction(Node::Ptr node)
{
    switch(node->mKind)
    {
        case Node::KIND_CONST:
        case Node::KIND_VAR:
        case Node::KIND_LIST:
            return ConstDummy;
        default:
        {
            for(auto& proc : AVAILABLE_PROCS)
            {
                if(node->mToken.compare(proc.procedure.id) == 0)
                {
                    return proc.func;
                }
            }
        }
    }
    assert(false);
    return nullptr;
}

void Interpreter::TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes)
{
    if (height > node->mHeight) node->mHeight = height;
    if(node->mKind != Node::KIND_LIST) necessaryNodes.insert(node);
    height++;
    for(auto parent : node->mParents)
    {
        TraverseNodes(parent, height, necessaryNodes);
    }
}

static size_t FindNodeOffset(const std::vector<Node::Ptr>& nodes, Node::Ptr node)
{
    return std::distance(nodes.begin(), std::find(std::begin(nodes), std::end(nodes), node));
}

void Interpreter::CompleteBuild()
{
    struct ObserverInfo
    {
        std::string token;
        uint16_t length;
        Node::Ptr node;
    };

    // Collect necessary nodes - nodes that are inputs to an observable
    // node. Also set the heights from observability
    std::set<Node::Ptr> necessaryNodes;
    std::vector<ObserverInfo> observers;

    for(auto ob : mGraph->GetObservers())
    {
        uint16_t length = 1;
        if(ob.second->mKind == Node::KIND_LIST)
        {
            auto numNodes = collectedObservers.size();
            CollectNodes(ob.second, collectedObservers);
            length = numNodes - collectedObservers.size();
        }
        else
        {
            uint64_t height=0;
            TraverseNodes(ob.second, height, necessaryNodes);
        }
        observers.push_back({ob.second, length, ob.first});
    }

    // Look for input lists and capture them to make sure we 
    // layout them out together
    std::vector<Node::Ptr> collectedInputs;
    std::vector<std::pair<std::string, Node::Ptr>> listInputs;
    for(auto in : mGraph->GetInputs())
    {
        if(in.second->mKind == Node::KIND_LIST)
        {
            auto numNodes = collectedInputs.size();
            CollectNodes(in.second, collectedInputs);
            listInputs.push_back(std::make_pair(in.first, *(collectedInputs.begin()+numNodes)));
        }
    }
    
    // Flatten collected nodes into continous block
    std::vector<Node::Ptr> nodeLayout;
    for(auto in : collectedInputs)
    {
        nodeLayout.push_back(in);
        necessaryNodes.erase(in);
    }
    for(auto n : necessaryNodes)
    {
        nodeLayout.push_back(n);
    }

    // For cache niceness
    mInterPointGraph.resize(nodeLayout.size()+collectedObservers.size());

    // Add list input extra name to map - A rather than A[0]
    for(auto li : listInputs)
    {
        size_t offset = FindNodeOffset(nodeLayout, li.second);
        auto& point = mInterPointGraph[offset];
        mInputs[li.first] = &point;
    }
    
    // Finish adding bulk of logic
    for(auto node : nodeLayout)
    {
        size_t offset = FindNodeOffset(nodeLayout, node);
        auto& point = mInterPointGraph[offset];

        point.mHeight = node->mHeight;

        for(auto pnode : node->mParents)
        {
            auto& parent = mInterPointGraph[FindNodeOffset(nodeLayout, pnode)];
            point.mParents.push_back(&parent);
            parent.mChildren.push_back(&point);
        }

        point.mComputeFunction = LookupComputeFunction(node);

        std::unordered_map<Node::Ptr, std::string>::iterator ob;
        if(node->mKind == Node::KIND_CONST)
        {
            point = std::stod(node->mToken);
        }
        else if(node->mIsInput)
        {
            mInputs[node->mToken] = &point;
        }
        if((ob = observers.find(node)) != observers.end())
        {
            mObservers[ob->second] = &point;
        }

        mRecomputeHeap.emplace(HeightPtrPair{point.mHeight, &point});
    }


    // if we have list observers increase height for everyone 
    // by one so we can have our observables copied
    for(auto &point : mInterPointGraph)
    {
        ++point.mHeight;
    }
    
    // Finish adding list observe copies
    int i = 0;
    for(auto node : collectedObservers)
    {
        auto& point = mInterPointGraph[nodeLayout.size()+i];
        auto& parent = mInterPointGraph[FindNodeOffset(nodeLayout, node)];
        point.mParents.push_back(&parent);
        parent.mChildren.push_back(&point);
        point.mHeight = 0;
        point.mComputeFunction = &Copy;
        i++;
    }

    // Add list observer extra name to map - A rather than A[0]
    for(auto ob : listObservers)
    {
        auto& point = mInterPointGraph[nodeLayout.size()+ob.second];
        mObservers[ob.first] = &point;
    }

    Stabilize();
}

bool Interpreter::IsDirty()
{
    for(const auto& namep : mInputs)
    {
        auto& interpoint = *namep.second;
        if(interpoint.mDirty) return true;
    }
    return false;
}

void Interpreter::Stabilize()
{
    for(const auto& namep : mInputs)
    {
        auto& interpoint = *namep.second;
        if(interpoint.mDirty)
        {
            for(auto* child : interpoint.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
            interpoint.mDirty = false;
        }
    }
    for(auto& hpp : mRecomputeHeap)
    {
        auto& point = *hpp.point;

        InterPoint old = point;
        point.mComputeFunction(point);

        if(old != point)
        {
            for(auto* child : point.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
        }
    }
    mRecomputeHeap.clear();
}

bool Interpreter::HasInputPoint(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end();
}

Point& Interpreter::LookupInputPoint(const std::string& label)
{
    assert(HasInputPoint(label));
    auto niter = mInputs.find(label);
    return *niter->second;
}

std::vector<std::string> Interpreter::GetInputPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mInputs)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Interpreter::DumpInputs()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mInputs)
    {
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

bool Interpreter::HasObserverPoint(const std::string& label)
{
    auto niter = mObservers.find(label);
    return niter != mObservers.end();
}

Point& Interpreter::LookupObserverPoint(const std::string& label)
{
    assert(HasObserverPoint(label));
    auto niter = mObservers.find(label);
    return *niter->second;
}

std::vector<std::string> Interpreter::GetObserverPointLabels()
{
    std::vector<std::string> ret;
    for(const auto& ip : mObservers)
    {
        ret.push_back(ip.first);
    }
    return ret;
}

std::unordered_map<std::string, double> Interpreter::DumpObservers()
{
    std::unordered_map<std::string, double> ret;
    for(const auto& ip : mObservers)
    {
        ret[ip.first] = ip.second->mVal;
    }
    return ret;
}

static std::unique_ptr<Graph> BuildAndLoadGraph()
{
    auto graph = std::make_unique<Graph>();
    std::vector<Procedure> procedures;
    for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

std::unique_ptr<IEngine> Interpreter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Construct(Parse(text));
    auto engine = std::make_unique<Interpreter>(std::move(graph));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

