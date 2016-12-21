
#include <cassert>
#include <sstream>
#include <set>

#include "graph.h"
#include "helpers.h"

// imports

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

template <typename T, size_t N>
void ValidateFunctionArgs(const std::string& name, const Node::Ptr node, const T (&args)[N])
{
    size_t argsize = node->mParents.size();
    if(argsize < N)
    {
        std::stringstream err;
        err << "Not enough items in list for function '" << name << 
            "'. Expected at least " << N << " Got " << argsize;
        throw GraphBuildException(err.str(), Cell());
    }

    if(argsize > N)
    {
        std::stringstream err;
        err << "Too many items in list for function '" << name <<
            "'. Expected at most " << N << " Got " << argsize;
        throw GraphBuildException(err.str(), Cell());
    }

    int i = 0;
    for(auto kind : args)
    {
        Node::Kind argKind = node->mParents[i]->mKind;
        if(kind != Node::Kind::KIND_UNKNOWN && !(kind & argKind))
        {
            std::stringstream err;
            err << "Incorrect argument " << i << " type for function '" << name <<
                "'. Got " << argKind << ". Expected " << kind;
            throw GraphBuildException(err.str(), Cell());
        }
        i++;
    }
}

Node::Ptr Graph::ForEach(Node::Ptr node)
{
    ValidateFunctionArgs("for-each", node, {KIND_PROC_FACTORY, KIND_LIST});
    auto args = node->mParents;
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
    ValidateFunctionArgs("map", node, {KIND_PROC_FACTORY, KIND_LIST});
    auto args = node->mParents;
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
    ValidateFunctionArgs("fold", node, {KIND_PROC_FACTORY, KIND_UNKNOWN, KIND_LIST});
    auto args = node->mParents;
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
    ValidateFunctionArgs("zip", node, {KIND_LIST, KIND_LIST});
    // Validate all lists same length
    // Validate all parents are lists
    auto args = node->mParents;
    auto zipped = BuildNode(KIND_LIST);
    auto& zparents = zipped->mParents;
    for(auto arg : args)
    {
        unsigned int i = 0;
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
    ValidateFunctionArgs("car", node, {KIND_LIST});
    auto arg = node->mParents[0];
    return arg->mParents[0];
}

Node::Ptr Graph::Cdr(Node::Ptr node)
{
    ValidateFunctionArgs("cdr", node, {KIND_LIST});
    auto arg = node->mParents[0];
    auto cdr = BuildNode(KIND_LIST);
    for(auto a = arg->mParents.begin() + 1; a != arg->mParents.end(); a++)
    {
        cdr->mParents.push_back(*a);
    }
    return cdr;
}

Node::Ptr Graph::Iota(Node::Ptr node)
{
    ValidateFunctionArgs("iota", node, {KIND_CONST, KIND_CONST, KIND_CONST});
    auto count = std::stod(node->mParents[0]->mToken);
    auto start = std::stod(node->mParents[1]->mToken);
    auto step = std::stod(node->mParents[2]->mToken);
    auto range = BuildNode(KIND_LIST);
    for(int i = 0; i < count; i++)
    {
        auto item = BuildNode(KIND_CONST);
        item->mToken = std::to_string(start+i*step);
        range->mParents.push_back(item);
    }
    return range;
}

Node::Ptr Graph::Apply(Node::Ptr node)
{
    //ValidateFunctionArgs("apply", node, 2);
    auto args = node->mParents;
    auto func = std::static_pointer_cast<ProcNodeFactory>(args[0]);

    auto nodes = BuildNode(KIND_LIST);
    for(auto arg = args.begin()+1;
            arg != args.end(); arg++)
    {
        auto& a = (*arg);
        if(a->mKind == Node::Kind::KIND_LIST)
        {
            for(auto n : a->mParents)
            {
                nodes->mParents.push_back(n);
            }
        }
        else
        {
            nodes->mParents.push_back(a);
        }
    }

    return func->mFactory(nodes);
}

Node::Ptr Graph::Append(Node::Ptr node)
{
    //ValidateFunctionArgs("apply", node, 2);
    auto args = node->mParents;

    auto nodes = BuildNode(KIND_LIST);
    for(auto arg = args.begin();
            arg != args.end(); arg++)
    {
        auto& a = (*arg);
        if(a->mKind == Node::Kind::KIND_LIST)
        {
            for(auto n : a->mParents)
            {
                nodes->mParents.push_back(n);
            }
        }
        else
        {
            nodes->mParents.push_back(a);
        }
    }

    return nodes;
}

Node::Ptr Graph::Import(Node::Ptr node)
{
    // Get library path
    // Search library paths
    // Build
}


#define WRAP(__FUNC) \
    [this](Node::Ptr ptr) -> Node::Ptr{return this->__FUNC(ptr);}

Graph::Graph(Graph* parent)
: Node(KIND_GRAPH) 
, mParent(parent)
{
    AddProcFactory("for-each",  WRAP(ForEach));
    AddProcFactory("map",       WRAP(Map));
    AddProcFactory("fold",      WRAP(Fold));
    AddProcFactory("list",      WRAP(List));
    AddProcFactory("zip",       WRAP(Zip));
    AddProcFactory("car",       WRAP(Car));
    AddProcFactory("cdr",       WRAP(Cdr));
    AddProcFactory("head",      WRAP(Car));
    AddProcFactory("rest",      WRAP(Cdr));
    AddProcFactory("iota",      WRAP(Iota));
    AddProcFactory("import",    WRAP(Import));
    AddProcFactory("apply",     WRAP(Apply));
    AddProcFactory("append",    WRAP(Append));
}

template<typename T, typename... Args>
std::shared_ptr<T> Graph::BuildNode(Args... as)
{
    auto ret = std::make_shared<T>(as...);
    mAllNodes.push_back(ret);
    return ret;
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

void Graph::BuildInputList(Node::Ptr child, std::string token, std::deque<int> dims)
{
    if(dims.size() == 0) return;

    int dim = dims.front();
    dims.pop_front();

    for(int i = 0; i < dim; i++)
    {
        auto interList = BuildNode(dims.empty() ? KIND_VAR : KIND_LIST);

        interList->mToken = token + "[" + std::to_string(i) + "]";
        interList->mInputLabels.push_back(interList->mToken);

        child->mParents.push_back(interList);
        DefineNode(interList->mToken, interList);
        BuildInputList(interList, interList->mToken, dims);
    }
}

void LabelObserver(Node::Ptr observer, std::string token)
{
    if(observer->mKind != Node::KIND_LIST)
    {
        observer->mObserverLabels.push_back(token);
        return;
    }

    int i = 0;
    for(auto parent : observer->mParents)
    {
        std::string label = token + "[" + std::to_string(i++) + "]";
        LabelObserver(parent, label);
    }
}

uint16_t GetListLength(Node::Ptr node)
{
    if(node->mKind != Node::KIND_LIST)
    {
        return 1;
    }

    uint16_t ret = 0;
    for(auto parent : node->mParents)
    {
        ret += GetListLength(parent);
    }
    return ret;
}

void Graph::LabelListRoot(Node::Ptr node, std::string label, uint16_t length, bool inputLabel)
{
    if(node->mKind != Node::KIND_LIST)
    {
        if(inputLabel)
        {
            node->mInputLabels.push_back(label);
        }
        else
        {
            node->mObserverLabels.push_back(label);
        }
        node->mLength = length;
        return;
    }
    if(node->mParents.size())
    {
        LabelListRoot(node->mParents[0], label, length, inputLabel);
    }
}

void Graph::Construct(const Cell &cell)
{
    Node::Ptr ret = nullptr;
    if(cell.type == Cell::Type::ROOT)
    {
        if(cell.list.size() == 0)
        {
            return;
        }
        for(const auto& c : cell.list)
        {
            const auto& l = c.list;
            if(l.size() > 1 && l[0].details.text == "begin")
            {
                Build(c);
                BuildLayout();
                return;
            }
        }
    }
    throw GraphBuildException("Construct not passed root cell from parser", cell);
}

Node::Ptr Graph::Build(const Cell &cell)
{
    Node::Ptr ret = nullptr;
    if(cell.type == Cell::Type::SYMBOL)
    {
        ret = LookupSymbol(cell);
    }
    else if(cell.type == Cell::Type::NUMBER)
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

                // Check valid type
                auto& inputTypeToken = cell.list[1].details.text;
                if(inputTypeToken.compare("list") == 0)
                {
                    //auto& listTypeToken = cell.list[2].details.text;
                    //auto listType = InputType2Enum(inputTypeToken);

                    // check input hasn't already been declared
                    auto& inputToken  = cell.list[3].details.text;
                    auto inputList = BuildNode(KIND_LIST);
                    inputList->mToken = inputToken;

                    DefineNode(inputToken, inputList);

                    // Add dimensions
                    std::deque<int> dims;
                    uint16_t length=1;
                    for(auto iter = cell.list.begin()+4;
                        iter != cell.list.end(); iter++)
                    {
                        const auto dim = std::stoi(iter->details.text);
                        dims.push_back(dim);
                        length *= dim;
                    }

                    BuildInputList(inputList, inputToken, dims);
                    LabelListRoot(inputList, inputToken, length, true);
                    inputList->mIsInput = true;
                }
                else
                {
                    //auto inputType = InputType2Enum(inputTypeToken);
            
                    // Add input node
                    for(auto iter = cell.list.begin()+2;
                        iter != cell.list.end(); iter++)
                    {
                        auto& inputToken = iter->details.text;

                        // check input hasn't already been declared
                        
                        auto inputNode = BuildNode(KIND_VAR);
                        inputNode->mToken = inputToken;
                        inputNode->mIsInput = true;

                        DefineNode(inputToken, inputNode);
                    }
                }
            }
            else if(firstElem.details.text == "observe")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                // Add check that we aren't already ouputing to this observer
                auto& varToken = cell.list[2];
                auto& outputToken = cell.list[1].details.text;

                // Register Observer
                auto varNode = Build(varToken);
                if(!varNode)
                {
                    throw GraphBuildException("Node isn't observerable", cell);
                }
                LabelObserver(varNode, outputToken);
                LabelListRoot(varNode, outputToken, GetListLength(varNode), false);
                varNode->mIsObserver = true;
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

std::string Graph::GetDOTGraph() const
{
    std::set<Node::Ptr> nodes;

    int i = 0;
    std::string ret = "{\n";
    for(auto nodeptr : mAllNodes)
    {
        nodes.insert(nodeptr);
        switch(nodeptr->mKind)
        {
            default: break;
            case KIND_CONST:
            case KIND_VAR:
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
        if(nodeptr->mIsObserver)
        {
            for(const auto& label : nodeptr->mObserverLabels)
            {
                ret += NodeToPtrString(nodeptr) + " -> " 

                    + std::to_string(i) + "\n" 
                    + std::to_string(i) + " [label=" + label
                    + "]\n";
            }
            ++i;
        }
    }

    for(auto nodeptr : nodes)
    {
        switch(nodeptr->mKind)
        {
            default: break;
            case KIND_GRAPH:
            case KIND_CONST:
            case KIND_VAR:
            case KIND_PROC:
            ret += NodeToDotLabel(nodeptr) + "\n"; break;
        }
    }

    ret += "}";
    return ret;
}

void TraverseNodes(Node::Ptr node, uint64_t& height, std::set<Node::Ptr>& necessaryNodes)
{
    if (height > node->mHeight) node->mHeight = height;
    necessaryNodes.insert(node);
    height++;
    for(auto parent : node->mParents)
    {
        TraverseNodes(parent, height, necessaryNodes);
    }
}

void CollectListMembers(Node::Ptr node, std::vector<Node::Ptr>& nodes)
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

void Graph::BuildLayout()
{
    // Collect observers including one level deep for list members
    std::vector<std::vector<Node::Ptr>> observers;
    for(const auto an : mAllNodes)
    {
        if(an->mIsObserver)
        {
            std::vector<Node::Ptr> nodes;
            CollectListMembers(an, nodes);
            observers.push_back(nodes);
        }
    }
    
    // Find the necessary nodes - nodes connected to an observer
    std::set<Node::Ptr> necessaryNodes;
    for(auto ob : observers)
    {
        for(auto node : ob)
        {
            // Start from one so list observer node copies can slot in beneath
            uint64_t height=1;
            TraverseNodes(node, height, necessaryNodes);
        }
    }

    // Flatten collected nodes into continous block
    // Step 1 - Add inputs
    for(auto an : mAllNodes)
    {
        if(an->mIsInput) CollectListMembers(an, mLayout);
    }

    // Step 2 - Remove inputs from necessary nodes
    for(auto n : mLayout)
    {
        necessaryNodes.erase(n);
    }
    
    // Step 3 - Add necessary nodes
    for(auto n : necessaryNodes)
    {
        mLayout.push_back(n);
    }

    // Step 4 - Add observer if necessary copies
    int i = 0;
    for(auto oi : observers)
    {
        if(oi.size() == 1)
        {
            mLayout.push_back(oi[0]);
        }
        else
        {
            for(auto node : oi)
            {
                auto nodeCopy = std::make_shared<Node>(Node::KIND_PROC);
                nodeCopy->mHeight = 0;
                nodeCopy->mToken = "copy";
                nodeCopy->mIsObserver = true;
                nodeCopy->mObserverLabels = node->mObserverLabels;
                nodeCopy->mInputLabels = node->mInputLabels;
                nodeCopy->mLength = node->mLength;
                nodeCopy->mParents.push_back(node);
                mLayout.push_back(nodeCopy);
                
                // No longer an observer
                node->mIsObserver = false; 
            }
        }
    }
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
