
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <cmath>

#include "interpreter.h"
#include "helpers.h"

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
    std::function<ComputeFunction ()> func;
};

void ConstDummy(InterPoint& /*point*/)
{
}

void Copy(InterPoint& point)
{
    assert(point.mParents.size() == 1);
    point = *point.mParents[0];
}

ComputeFunction Latch()
{
    Point lastPoint;
    return [lastPoint](InterPoint& point) mutable
    {
        assert(point.mParents.size() == 2);
        if (point.mParents[0]->mVal)
        {
            lastPoint = *point.mParents[1];
            point = lastPoint;
        }
    };
}

ComputeFunction FlipFlop()
{
    Point lastPoint;
    return [lastPoint](InterPoint& point) mutable
    {
        assert(point.mParents.size() == 2);
        if (point.mParents[0]->mVal)
        {
            point = lastPoint;
            lastPoint = *point.mParents[1];
        }
    };
}

ComputeFunction Tick()
{
    uint64_t tick=0;
    return [tick](InterPoint& point) mutable
    {
        point = ++tick;
    };
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
    point = *(*p);
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

std::function<ComputeFunction ()> Wrap(ComputeFunction func)
{
    return [func]() -> ComputeFunction {return func;};
}

static InterPointProcessor AVAILABLE_PROCS[] =
{
    {{"?",          CountValueValidator<3,3>},  Wrap(Ternary)},
    {{"+",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::plus<double>>)},
    {{"-",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::minus<double>>)},
    {{"/",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::divides<double>>)},
    {{"*",          MinCountValueValidator<2>},  Wrap(LoopOperator<std::multiplies<double>>)},
    //{{"%",          DummyValidator},Wrap(  LoopOperator<std::modulus<double>>)},
    {{"<",          CountValueValidator<2,2>},  Wrap(PairOperator<std::less<double>>)},
    {{"<=",         CountValueValidator<2,2>},  Wrap(PairOperator<std::less_equal<double>>)},
    {{">",          CountValueValidator<2,2>},  Wrap(PairOperator<std::greater<double>>)},
    {{">=",         CountValueValidator<2,2>},  Wrap(PairOperator<std::greater_equal<double>>)},
    {{"==",         CountValueValidator<2,2>},  Wrap(PairOperator<std::equal_to<double>>)},
    {{"!=",         CountValueValidator<2,2>},  Wrap(PairOperator<std::not_equal_to<double>>)},
    {{"&&",         CountValueValidator<2,2>},  Wrap(PairOperator<std::logical_and<double>>)},
    {{"||",         CountValueValidator<2,2>},  Wrap(PairOperator<std::logical_or<double>>)},
    {{"min",        MinCountValueValidator<2>},  Wrap(LoopOperator<MinFunc>)},
    {{"max",        MinCountValueValidator<2>},  Wrap(LoopOperator<MaxFunc>)},
    {{"exp",        CountValueValidator<1,1>},  Wrap(UnaryOperator<ExpFunc>)},
    {{"ln",         CountValueValidator<1,1>},  Wrap(UnaryOperator<LogFunc>)},
    {{"not",        CountValueValidator<1,1>},  Wrap(UnaryOperator<std::logical_not<double>>)},
    {{"latch",      CountValueValidator<2,2>},  Latch},
    {{"flip-flop",  CountValueValidator<2,2>},  FlipFlop},
    {{"tick",       MinCountValueValidator<0>}, Tick},
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
                    return proc.func();
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
    necessaryNodes.insert(node);
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

void Interpreter::CollectListMembers(Node::Ptr node, std::vector<Node::Ptr>& nodes)
{
    if(node->mKind != Node::KIND_LIST)
    {
        nodes.push_back(node);
        return;
    }
    for(auto parent : node->mParents)
    {
        CollectListMembers(parent, nodes);
    }
}

void Interpreter::CompleteBuild()
{
    struct NodeInfo
    {
        std::string token;
        std::vector<Node::Ptr> nodes;
    };

    // Collect necessary nodes - nodes that are inputs to an observable
    // node. Also set the heights from observability
    std::set<Node::Ptr> necessaryNodes;
    std::vector<NodeInfo> observersInfo;
    int numListObserverElements = 0;

    for(auto ob : mGraph->GetObservers())
    {
        NodeInfo ni; 
        ni.token = ob.first;
        CollectListMembers(ob.second, ni.nodes);
        observersInfo.push_back(ni);
        numListObserverElements += ni.nodes.size();
    }
    
    for(auto ob : observersInfo)
    {
        for(auto node : ob.nodes)
        {
            // Start from one so observer nodes can slot in beneath
            uint64_t height=1;
            TraverseNodes(node, height, necessaryNodes);
        }
    }

    // Look for input lists and capture them to make sure we 
    // layout them out together
    std::vector<NodeInfo> inputsInfo;
    for(auto in : mGraph->GetInputs())
    {
        NodeInfo ni; 
        ni.token = in.first;
        CollectListMembers(in.second, ni.nodes);
        inputsInfo.push_back(ni);
    }
    
    // Flatten collected nodes into continous block
    std::vector<Node::Ptr> nodeLayout;
    for(auto ii : inputsInfo)
    {
        // Lazy here - rather than only layout input
        // lists in continous memory we group all 
        // inputs together
        for(auto in : ii.nodes)
        {
            nodeLayout.push_back(in);
            necessaryNodes.erase(in);
        }
    }
    for(auto n : necessaryNodes)
    {
        nodeLayout.push_back(n);
    }

    // For cache niceness
    mInterPointGraph.resize(nodeLayout.size()+numListObserverElements);

    // Add list input extra name to map - A rather than A[0]
    // Add list input max length
    for(auto ii : inputsInfo)
    {
        if(ii.nodes.size())
        {
            size_t offset = FindNodeOffset(nodeLayout, ii.nodes.front());
            auto& point = mInterPointGraph[offset];
            mInputs[ii.token] = &point;
            point.mLength = ii.nodes.size();
        }
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
    for(auto oi : observersInfo)
    {
        if(oi.nodes.size() > 1)
        {
            // Add list observer extra name to map - A rather than A[0]
            mObservers[oi.token] = &mInterPointGraph[nodeLayout.size()+i];
            mObservers[oi.token]->mLength = oi.nodes.size();
            int k = 0;
            for(auto node : oi.nodes)
            {
                // Copies 
                auto& point = mInterPointGraph[nodeLayout.size()+i];
                auto& parent = mInterPointGraph[FindNodeOffset(nodeLayout, node)];
                point.mParents.push_back(&parent);
                parent.mChildren.push_back(&point);
                point.mHeight = 0;
                point.mComputeFunction = &Copy;
                mObservers[oi.token + "[" + std::to_string(k) + "]"] = &point;
                mRecomputeHeap.emplace(HeightPtrPair{point.mHeight, &point});
                ++i;
                ++k;
            }
        }
        else if(oi.nodes.size())
        {
            // add in reference for single observe
            auto& point = mInterPointGraph[FindNodeOffset(nodeLayout, oi.nodes.front())];
            mObservers[oi.token] = &point;
        }
    }

    Stabilize();
}

bool Interpreter::IsDirty()
{
    for(const auto& namep : mInputs)
    {
        auto& interpoint = *namep.second;
        if(interpoint.IsDirty()) return true;
    }
    return false;
}

void Interpreter::Stabilize(bool force)
{
    for(const auto& namep : mInputs)
    {
        auto& interpoint = *namep.second;
        if(force || interpoint.IsDirty())
        {
            for(auto* child : interpoint.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
            interpoint.Clean();
        }
    }
    for(auto& hpp : mRecomputeHeap)
    {
        auto& point = *hpp.point;
        point.mComputeFunction(point);
        if(point.IsDirty())
        {
            for(auto* child : point.mChildren)
            {
                mRecomputeHeap.emplace(HeightPtrPair{child->mHeight, child});
            }
            point.Clean();
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
    auto graph = std::unique_ptr<Graph>(new Graph);
    std::vector<Procedure> procedures;
    for(const auto &proc : AVAILABLE_PROCS) procedures.push_back(proc.procedure);
    graph->SetSupportedProcedures(procedures);
    return graph;
}

std::unique_ptr<IEngine> Interpreter::Build(const std::string& text)
{
    auto graph = BuildAndLoadGraph();
    graph->Construct(Parse(text));
    auto engine = std::unique_ptr<Interpreter>(new Interpreter(std::move(graph)));
    engine->CompleteBuild();
    return std::unique_ptr<IEngine>(std::move(engine));
}

}

