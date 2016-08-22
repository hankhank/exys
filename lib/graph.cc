
#include <cassert>
#include <sstream>
#include <set>

#include "graph.h"
#include "helpers.h"

// Things to test
// zipping lists together in mini lists - done
// variable length lists
// memory - flipflop
// timers/decay

namespace Exys
{

void ValidateListLength(const Cell& cell, size_t min, size_t max=std::numeric_limits<size_t>::max())
{
    if(cell.list.size() < min)
    {
        std::stringstream err;
        err << "Not enough items in list for function. Expected at least "
            << min << " Got " << cell.list.size();
        throw GraphBuildException(err.str(), cell);
    }
    if(cell.list.size() > max)
    {
        std::stringstream err;
        err << "Too many items in list for function. Expected at most "
            << max << " Got " << cell.list.size();
        throw GraphBuildException(err.str(), cell);
    }
}

void ValidateParamListLength(const std::vector<Cell> params1, const Node::Ptr node)
{
    if(params1.size() != node->mParents.size())
    {
        Cell cell;
        std::stringstream err;
        err << "Incorrect number of params. Expected "
            << params1.size() << " Got " << node->mParents.size();
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

Node::Ptr Graph::Fold(Node::Ptr node)
{
    //ValidateListLength(args, 3);
    auto args = node->mParents;
    assert(args[0]->mKind == KIND_PROC_FACTORY);
    assert(args[2]->mKind == KIND_LIST);
    auto func = std::static_pointer_cast<ProcNodeFactory>(args[0]);
    auto last = std::static_pointer_cast<Node>(args[1]);
    auto exps = std::static_pointer_cast<Node>(args[2]);
    for(auto exp : exps->mParents)
    {
        auto tmp = std::make_shared<Node>(KIND_LIST);
        tmp->mParents.push_back(last);
        tmp->mParents.push_back(exp);
        last = func->mFactory(tmp);
    }

    return last;
}

Node::Ptr Graph::List(Node::Ptr node)
{
    return node;
}

Node::Ptr Graph::Zip(Node::Ptr node)
{
    //ValidateListLength(args, 3);
    // Validate all lists same length
    // Validate all parents are lists
    auto args = node->mParents;
    auto zipped = BuildNode(KIND_LIST);
    auto& zparents = zipped->mParents;
    for(auto arg : args)
    {
        int i = 0;
        for(auto a : arg->mParents)
        {   
            if(i >= zparents.size())
            {
                zparents.push_back(BuildNode(KIND_LIST));
            }
            zparents[i]->mParents.push_back(a);
            ++i;
        }
    }
    return zipped;
}

Node::Ptr Graph::Car(Node::Ptr node)
{
    // Validate arg list is 1 long
    // Validate node is list
    // Validate list is at least 1 long
    auto arg = node->mParents[0];
    return arg->mParents[0];
}

Node::Ptr Graph::Cdr(Node::Ptr node)
{
    // Validate arg list is 1 long
    // Validate node is list
    // Validate list is at least 1 long
    auto arg = node->mParents[0];
    auto cdr = BuildNode(KIND_LIST);
    for(auto a = arg->mParents.begin() + 1; a != arg->mParents.end(); a++)
    {
        cdr->mParents.push_back(*a);
    }
    return cdr;
}

#define WRAP(__FUNC) \
    [this](Node::Ptr ptr) -> Node::Ptr{return this->__FUNC(ptr);}

Graph::Graph(Graph* parent)
: Node(KIND_GRAPH) 
, mParent(parent)
{
    AddProcFactory("for-each", WRAP(ForEach));
    AddProcFactory("map",      WRAP(Map));
    AddProcFactory("fold",     WRAP(Fold));
    AddProcFactory("list",     WRAP(List));
    AddProcFactory("zip",      WRAP(Zip));
    AddProcFactory("car",      WRAP(Car));
    AddProcFactory("cdr",      WRAP(Cdr));
    AddProcFactory("head",     WRAP(Car));
    AddProcFactory("rest",     WRAP(Cdr));
}

template<typename T, typename... Args>
std::shared_ptr<T> Graph::BuildNode(Args... as)
{
    auto ret = std::make_shared<T>(as...);
    mAllNodes.push_back(ret);
    return ret;
}

const std::unordered_map<std::string, Node::Ptr>& Graph::GetObservers()
{
    return mObservers;
}

void Graph::SetSupportedProcedures(const std::vector<Procedure>& procs)
{
    for(const auto& proc : procs) AddProcFactory(proc.id, DefaultFactory(proc));
}

ProcNodeFactoryFunc Graph::DefaultFactory(const Procedure& procedure)
{
    return [this, procedure](Node::Ptr node) -> Node::Ptr
    {
        procedure.validate(node);
        auto mn = BuildNode(KIND_PROC);
        mn->mToken = procedure.id;
        for(auto& n : node->mParents)
        {
            mn->mParents.push_back(n);
        }
        return mn;
    };
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
                        ValidateParamListLength(params, node);

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
                auto varNode = Build(varToken);
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

GraphBuildException::GraphBuildException(const std::string& error, Cell cell)
: mError(error)
, mCell(cell)
{
}

const char* GraphBuildException::what() const noexcept(true)
{
    return mError.c_str();
}

std::string GraphBuildException::GetErrorMessage(const std::string& inputText) const
{
    std::string errmsg;
    errmsg += "Line " + std::to_string(mCell.details.firstLineNumber+1);
    errmsg += ": Error: ";
    errmsg += mError;
    errmsg += "\n";
    errmsg += GetLine(inputText, mCell.details.firstLineNumber);
    errmsg += "\n";
    for(int i = 0; i < mCell.details.firstColumn; i++)
    {
        errmsg += " ";
    }
    errmsg += "^\n";
    return errmsg;
}


}
