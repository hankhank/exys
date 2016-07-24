
#include <exys.h>

#include <set>

// debug headers
#include <iostream>
#include <sstream>

namespace Exys
{

template<typename T, typename... Args>
std::shared_ptr<T> Graph::BuildNode(Args... as)
{
    auto ret = std::make_shared<T>(as...);
    mAllNodes.push_back(ret);
    return ret;
}

template<typename T>
Node::Ptr Graph::BuildForProc(const std::vector<Cell>& args)
{
    auto mn = BuildNode<T>();
    for(auto& arg : args)
    {
        auto leg = Build(arg);
        mn->mParents.push_back(leg);
    }
    return mn;
}

#define ADD_PROC(__ID, __BUILDER) \
    { \
        auto pnode = BuildNode<ProcNode>(); \
        pnode->mFactory =  \
        [this](std::vector<Cell> args)->Node::Ptr \
                        {return this->__BUILDER(args);}; \
        mVarNodes[std::string(__ID)] = pnode; \
    }

Graph::Graph(Graph* parent)
: Node(KIND_GRAPH) 
, mParent((Graph*)parent)
{
    ADD_PROC("+", BuildForProc<SumNode>);
    ADD_PROC("-", BuildForProc<SubNode>);
    ADD_PROC("/", BuildForProc<DivNode>);
    ADD_PROC("*", BuildForProc<MulNode>);
}

Node::Ptr Graph::LookupSymbol(const Cell& cell)
{
    auto niter = mVarNodes.find(cell.token);
    if (niter != mVarNodes.end())
    {
        return niter->second;
    }
    if(mParent) return mParent->LookupSymbol(cell);

    std::stringstream err;
    err << "Could not find symbol - " << cell.token;
    throw GraphBuildException(err.str(), cell);

    return nullptr;
}

void Graph::SetSymbol(const Cell& cell, Node::Ptr node)
{
    auto niter = mVarNodes.find(cell.token);
    if (niter != mVarNodes.end())
    {
        niter->second = node;
        return;
    }
    if(mParent) return mParent->SetSymbol(cell, node);

    std::stringstream err;
    err << "Could not find symbol - " << cell.token;
    throw GraphBuildException(err.str(), cell);
}

SubGraphFactory Graph::LookupProcedure(const Cell& cell)
{
    auto niter = mVarNodes.find(cell.token);
    if (niter == mVarNodes.end())
    {
        std::stringstream err;
        err << "Could not find procedure - " << cell.token;
        throw GraphBuildException(err.str(), cell);
    }
    if (niter->second->mKind != Node::KIND_PROC)
    {
        std::stringstream err;
        err << "Not a valid procedure - " << cell.token;
        throw GraphBuildException(err.str(), cell);
    }
    return static_cast<ProcNode*>(niter->second.get())->mFactory;
}

Node::Type Graph::InputType2Enum(const std::string& token)
{
    return Node::Type::TYPE_DOUBLE;
}

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

void Graph::DefineNode(const std::string& token, const Cell& exp)
{
    auto parent = Build(exp);
    mVarNodes[token] = parent;
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
        auto cnode = BuildNode<ConstNode>();
        cnode->ReadConstant(cell.token);
        ret = cnode;
    }
    else if(cell.type == Cell::Type::LIST)
    {
        if (!cell.list.empty())
        {
            auto& firstElem = cell.list.front();
            if(firstElem.token == "?")
            {
                ValidateListLength(cell, 4);

                auto& condExp = cell.list[1];
                auto& posExp  = cell.list[2];
                auto& negExp  = cell.list[3];
                
                auto ifNode = BuildNode<TenaryNode>();
                auto condNode = Build(condExp);
                auto posNode = Build(posExp);
                auto negNode = Build(negExp);

                // Check neg and pos legs have same type
                ifNode->SetLegs(condNode, posNode, negNode);

                ret = ifNode;
            }
            else if(firstElem.token == "begin")
            {
                ValidateListLength(cell, 2);

                std::vector<Cell> cells(cell.list.begin()+1, cell.list.end()); 
                for(auto c : cells)
                {
                    ret = Build(c);
                }
            }
            else if(firstElem.token == "define")
            {
                ValidateListLength(cell, 3);

                auto& varToken = cell.list[1].token;
                auto& exp = cell.list[2];

                // check whether its been defined in this scope

                // Build parents and adopt their type
                DefineNode(varToken, exp);
            }
            else if(firstElem.token == "set!")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                auto& var = cell.list[1];
                auto& exp = cell.list[2];

                // Build parents and adopt their type
                auto parent = Build(exp);
                SetSymbol(var, parent);
            }
            else if(firstElem.token == "lambda")
            {
                ValidateListLength(cell, 3);
                // Think I need to capture tree here
                // and build graph whenever invoked
                // I think this will be the same for
                // all procs
                // For map
 
                // Add check for list length
                // Add check whether token in list send warn
                auto params = cell.list[1].list;
                auto exp = cell.list[2];
                
                auto pnode = BuildNode<ProcNode>();
                pnode->mFactory = 
                [this, params, exp](std::vector<Cell> args)
                {
                    ValidateParamListLength(params, args);

                    auto newSubGraph = BuildNode<Graph>(this);
                    for(size_t i = 0; i < params.size(); i++)
                    {
                        newSubGraph->DefineNode(params[i].token,
                                args[i]);
                    }
                    return newSubGraph->Build(exp);
                };
                ret = pnode;
            }
            else if(firstElem.token == "input")
            {
                ValidateListLength(cell, 3);

                // Add check whether token in list send warn
                //
                auto& inputTypeToken = cell.list[1].token;

                // Check valid type
                auto inputType = InputType2Enum(inputTypeToken);
        
                // Add input node
                for(auto iter = cell.list.begin()+2;
                    iter != cell.list.end(); iter++)
                {
                    auto& inputToken = iter->token;

                    // check input hasn't already been declared
                    
                    auto inputNode = BuildNode<InputNode>();
                    inputNode->mType = inputType;
                    inputNode->mToken = inputToken;

                    mVarNodes[inputToken] = inputNode;
                    mInputs[inputToken] = inputNode;
                }
            }
            else if(firstElem.token == "observe")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                // Add check that we aren't already ouputing to this observer
                auto& varToken = cell.list[1];
                auto& outputToken = cell.list[2].token;

                // Register Observer
                auto varNode = LookupSymbol(varToken);
                mObservers[outputToken] = varNode;
            }
            else // procedure call
            {
                // Add check for list length
                auto proc = LookupProcedure(firstElem);
                std::vector<Cell> args(cell.list.begin()+1, cell.list.end()); 
                ret = proc(args);
            }
        }
    }
    return ret;
}

void Graph::CompleteBuild()
{
    // Set heights add children to necessary nodes
    //for(auto ob : mObservers)
    //{
    //    uint64_t height = 0;
    //    auto process = [this](Node::Ptr node, uint64_t& height)
    //    {
    //        node->mHeight = height++;
    //        mNecessaryNodes.push_back(node);
    //        for(auto parent : node->mParents)
    //        {
    //            process(parent, height);
    //        }
    //    }
    //    process(ob);
    //}
}

Node::Ptr Graph::LookupInputNode(const std::string& label)
{
    auto niter = mInputs.find(label);
    return niter != mInputs.end() ? niter->second : nullptr;
}

Node::Ptr Graph::LookupObserverNode(const std::string& label)
{
    auto niter = mObservers.find(label);
    return niter != mObservers.end() ? niter->second : nullptr;
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
            case KIND_PROC: break; // NOP
            case KIND_GRAPH:
            {
                auto subgraph = static_cast<Graph*>(nodeptr.get());
                ret += "subgraph " + std::to_string((long int) subgraph) + " ";
                ret +=  subgraph->GetDOTGraph();
                ret += "\n";
            }
            break;

            default:
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
        //std::cout << nodeptr->mKind << "\n";
        //std::cout << (long int)nodeptr.get() << "\n";
        switch(nodeptr->mKind)
        {
            case KIND_PROC: break; // NOP
            case KIND_GRAPH: break; // NOP
            default: ret += NodeToDotLabel(nodeptr) + "\n"; break;
        }
    }

    ret += "}";
    return ret;
}

std::unique_ptr<Graph> Graph::BuildGraph(const std::string& text)
{
    auto graph = std::make_unique<Graph>();
    graph->Build(Parse(text));
    graph->CompleteBuild();
    return graph;
}

}

