
#include <cassert>
#include <sstream>
#include <set>

#include "graph.h"

namespace Exys
{
void ValidateListLength(const Cell& cell, size_t min)
{
    if(cell.list.size() < min)
    {
        std::stringstream err;
        err << "List length too small. Expected at least "
            << min << " Got " << cell.list.size();
        throw GraphBuildException(err.str(), cell);
    }
}

void ValidateParamListLength(const std::vector<Cell> params1,
    const std::vector<Cell> params2)
{
    if(params1.size() != params2.size())
    {
        Cell cell;
        std::stringstream err;
        err << "Incorrect number of params. Expected "
            << params1.size() << " Got " << params2.size();
        throw GraphBuildException(err.str(), cell);
    }
}

Node::Ptr Graph::ForEach(Node::Ptr node)
{
    //ValidateListLength(args, 3);
    auto args = node->mParents;
    assert(args[0]->mKind == KIND_PROC_FACTORY);
    assert(args[1]->mKind == KIND_LIST);
    auto func = std::static_pointer_cast<ProcNodeFactory>(args[0]);
    auto exps = std::static_pointer_cast<Node>(args[1]);
    for(auto exp : exps->mParents)
    {
        auto tmp = std::make_shared<Node>(KIND_LIST);
        tmp->mParents.push_back(exp);
        func->mFactory(tmp);
    }
    return nullptr;
}

Node::Ptr Graph::Map(Node::Ptr node)
{
    //ValidateListLength(args, 3);
    auto args = node->mParents;
    assert(args[0]->mKind == KIND_PROC_FACTORY);
    assert(args[1]->mKind == KIND_LIST);
    auto func = std::static_pointer_cast<ProcNodeFactory>(args[0]);
    auto exps = std::static_pointer_cast<Node>(args[1]);
    auto mapped = BuildNode(KIND_LIST);
    for(auto exp : exps->mParents)
    {
        auto tmp = std::make_shared<Node>(KIND_LIST);
        tmp->mParents.push_back(exp);
        mapped->mParents.push_back(func->mFactory(tmp));
    }
    return mapped;
}

Node::Ptr Graph::List(Node::Ptr node)
{
    return node;
}

#define WRAP(__FUNC) \
    [this](Node::Ptr ptr) -> Node::Ptr{return this->__FUNC(ptr);}


Graph::Graph(Graph* parent)
: Node(KIND_GRAPH) 
, mParent(parent)
{
    AddProcFactory("for-each", WRAP(ForEach));
    AddProcFactory("map",      WRAP(Map));
    AddProcFactory("list",     WRAP(List));
}

template<typename T, typename... Args>
std::shared_ptr<T> Graph::BuildNode(Args... as)
{
    auto ret = std::make_shared<T>(as...);
    mAllNodes.push_back(ret);
    return ret;
}

ProcNodeFactoryFunc Graph::DefaultFactory(const std::string& procname)
{
    return [this, procname](Node::Ptr node) -> Node::Ptr
    {
        auto mn = BuildNode(KIND_PROC);
        mn->mToken = procname;
        for(auto& n : node->mParents)
        {
            mn->mParents.push_back(n);
        }
        return mn;
    };
}
const std::unordered_map<std::string, Node::Ptr>& Graph::GetObservers()
{
    return mObservers;
}

void Graph::SetSupportedProcedures(const std::vector<std::string>& procs)
{
    for(const auto& proc : procs) AddProcFactory(proc, DefaultFactory(proc));
}

void Graph::AddProcFactory(const std::string id, ProcNodeFactoryFunc factory)
{
    auto pnode = BuildNode<ProcNodeFactory>(factory);
    mVarNodes[id] = pnode;
}

void Graph::SetSymbol(const Cell& cell, Node::Ptr node)
{
    auto niter = mVarNodes.find(cell.details.text);
    if (niter != mVarNodes.end())
    {
        niter->second = node;
        return;
    }
    if(mParent) return mParent->SetSymbol(cell, node);

    std::stringstream err;
    err << "Could not find symbol - " << cell.details.text;
    throw GraphBuildException(err.str(), cell);
}

Node::Ptr Graph::LookupSymbol(const Cell& cell)
{
    auto niter = mVarNodes.find(cell.details.text);
    if (niter != mVarNodes.end())
    {
        return niter->second;
    }
    if(mParent) return mParent->LookupSymbol(cell);

    std::stringstream err;
    err << "Could not find symbol - " << cell.details.text;
    throw GraphBuildException(err.str(), cell);

    return nullptr;
}

ProcNodeFactoryFunc Graph::LookupProcedure(const Cell& cell)
{
    auto node = LookupSymbol(cell);
    if (node->mKind != Node::KIND_PROC_FACTORY)
    {
        std::stringstream err;
        err << "Not a valid procedure - " << cell.details.text;
        throw GraphBuildException(err.str(), cell);
    }
    return static_cast<ProcNodeFactory*>(node.get())->mFactory;
}

Node::Type Graph::InputType2Enum(const std::string& token)
{
    return Node::Type::TYPE_DOUBLE;
}

void Graph::DefineNode(const std::string& token, const Cell& exp)
{
    auto parent = Build(exp);
    mVarNodes[token] = parent;
}

void Graph::DefineNode(const std::string& token, Node::Ptr node)
{
    mVarNodes[token] = node;
}

Node::Ptr Graph::Build(const Cell &cell)
{
    Node::Ptr ret = nullptr;
    if(cell.type == Cell::Type::SYMBOL)
    {
        ret = LookupSymbol(cell);
    }
    if(cell.type == Cell::Type::NUMBER)
    {
        auto cnode = BuildNode(KIND_CONST);
        cnode->mToken = cell.details.text;
        ret = cnode;
    }
    else if(cell.type == Cell::Type::LIST)
    {
        if (!cell.list.empty())
        {
            auto& firstElem = cell.list.front();
            if(firstElem.details.text == "begin")
            {
                ValidateListLength(cell, 2);

                for(auto c = cell.list.begin()+1; c != cell.list.end(); c++)
                {
                    ret = Build(*c);
                }
            }
            else if(firstElem.details.text == "define")
            {
                ValidateListLength(cell, 3);

                auto& varToken = cell.list[1].details.text;
                auto& exp = cell.list[2];

                // check whether its been defined in this scope

                // Build parents and adopt their type
                DefineNode(varToken, exp);
            }
            else if(firstElem.details.text == "set!")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                auto& var = cell.list[1];
                auto& exp = cell.list[2];

                // Build parents and adopt their type
                auto parent = Build(exp);
                SetSymbol(var, parent);
            }
            else if(firstElem.details.text == "lambda")
            {
                ValidateListLength(cell, 3);
                // Add check for list length
                // Add check whether token in list send warn
                auto params = cell.list[1].list;
                auto exp = cell.list[2];
                
                auto pnode = BuildNode<ProcNodeFactory>
                (
                    [this, params, exp](Node::Ptr node)
                    {
                        //ValidateParamListLength(params, args);

                        auto newSubGraph = BuildNode<Graph>(this);
                        for(size_t i = 0; i < params.size(); i++)
                        {
                            newSubGraph->DefineNode(params[i].details.text,
                                    node->mParents[i]);
                        }
                        return newSubGraph->Build(exp);
                    }
                );
                ret = pnode;
            }
            else if(firstElem.details.text == "input")
            {
                ValidateListLength(cell, 3);

                // Add check whether token in list send warn
                auto& inputTypeToken = cell.list[1].details.text;

                // Check valid type
                auto inputType = InputType2Enum(inputTypeToken);
        
                // Add input node
                for(auto iter = cell.list.begin()+2;
                    iter != cell.list.end(); iter++)
                {
                    auto& inputToken = iter->details.text;

                    // check input hasn't already been declared
                    
                    auto inputNode = BuildNode(KIND_INPUT);
                    inputNode->mToken = inputToken;

                    DefineNode(inputToken, inputNode);
                }
            }
            else if(firstElem.details.text == "observe")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                // Add check that we aren't already ouputing to this observer
                auto& varToken = cell.list[1];
                auto& outputToken = cell.list[2].details.text;

                // Register Observer
                auto varNode = LookupSymbol(varToken);
                mObservers[outputToken] = varNode;
            }
            else // procedure call
            {
                auto node = BuildNode(KIND_LIST);
                for(auto arg = cell.list.begin()+1;
                        arg != cell.list.end(); arg++)
                {
                    node->mParents.push_back(Build(*arg));
                }

                auto proc = LookupProcedure(firstElem);
                ret = proc(node);
            }
        }
    }
    return ret;
}

std::string NodeToPtrString(Node::Ptr node)
{
    return std::to_string((long int)node.get());
}

std::string NodeToDotLabel(Node::Ptr node)
{
    return std::to_string((long int)node.get())
        + "[label=\"" + node->Label() + "\"];";
}

std::string Graph::GetDOTGraph()
{
    std::set<Node::Ptr> nodes;

    std::string ret = "{\n";
    for(auto nodeptr : mAllNodes)
    {
        nodes.insert(nodeptr);
        switch(nodeptr->mKind)
        {
            default: break;
            case KIND_GRAPH:
            {
                auto subgraph = std::static_pointer_cast<Graph>(nodeptr);
                ret += "subgraph " + std::to_string((long int) subgraph.get()) + " ";
                ret +=  subgraph->GetDOTGraph();
                ret += "\n";
            }
            break;

            case KIND_CONST:
            case KIND_INPUT:
            case KIND_PROC:
            {
                auto childLabel = NodeToPtrString(nodeptr);
                for(auto parent : nodeptr->mParents)
                {
                    nodes.insert(parent);
                    ret += NodeToPtrString(parent) + " -> " 
                        + childLabel + "\n";
                }
            }
            break;
        }
    }

    int i = 0;
    for(auto tokennode : mObservers)
    {
        ret += NodeToPtrString(tokennode.second) + " -> " 
            + std::to_string(i) + "\n" 
            + std::to_string(i) + " [label=" + tokennode.first 
            + "]\n";
        ++i;
    }

    for(auto nodeptr : nodes)
    {
        switch(nodeptr->mKind)
        {
            default: break;

            case KIND_GRAPH:
            case KIND_CONST:
            case KIND_INPUT:
            case KIND_PROC:
            ret += NodeToDotLabel(nodeptr) + "\n"; break;
        }
    }

    ret += "}";
    return ret;
}

}
