
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>

#include "graph.h"
#include "helpers.h"
#include "std/stdlib.h"

namespace
{
    constexpr char EXYS_REQUIRE_PATH[] = "EXYS_REQUIRE_PATH";
}

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

template <int N>
void ValidateMinListLength(const Node::Ptr node)
{
    size_t argsize = node->mParents.size();
    if(argsize < N)
    {
        std::stringstream err;
        err << "Not enough items in list."
            " Expected at least " << N << " Got " << argsize;
        throw GraphBuildException(err.str(), Cell());
    }
}

template <int N>
void ValidateListLength(const Node::Ptr node)
{
    size_t argsize = node->mParents.size();
    if(argsize > N)
    {
        std::stringstream err;
        err << "Too many arguments."
            " Expected " << N << " Got " << argsize;
        throw GraphBuildException(err.str(), Cell());
    }
    else if(argsize < N)
    {
        std::stringstream err;
        err << "Too few arguments."
            " Expected " << N << " Got " << argsize;
        throw GraphBuildException(err.str(), Cell());
    }
}

template <typename T>
void ValidateFunctionArgs(const std::string& name, const Node::Ptr node, const std::initializer_list<T> args)
{
    size_t argsize = node->mParents.size();
    if(argsize < args.size())
    {
        std::stringstream err;
        err << "Not enough items in list for function '" << name << 
            "'. Expected at least " << args.size() << " Got " << argsize;
        throw GraphBuildException(err.str(), Cell());
    }

    if(argsize > args.size())
    {
        std::stringstream err;
        err << "Too many items in list for function '" << name <<
            "'. Expected at most " << args.size() << " Got " << argsize;
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
    ValidateMinListLength<1>(arg);
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

Node::Ptr Graph::Nth(Node::Ptr node)
{
    ValidateFunctionArgs("nth", node, {KIND_CONST, KIND_LIST});
    auto nth = std::stod(node->mParents[0]->mToken);
    auto list = std::static_pointer_cast<Node>(node->mParents[1]);

    int i = 0;
    for(auto item : list->mParents)
    {
        if(i == nth) return item;
        ++i;
    }

    std::stringstream err;
    err << "Not enough items in list. Expected at least " << nth 
        << " Got " << list->mParents.size();

    throw GraphBuildException(err.str(), Cell());

    return nullptr;
}

// Printf functionality
// %[flags][width][.precision][length]specifier
const char* flags = "-+0 #";
const char* widthPrec = "0123456789.,*";
const char* modifiers = "hlLzjt";
const char* types = "diufFeEgGxXaAoscpn";

Node::Ptr Graph::Format(Node::Ptr node)
{
    auto formatargs = node->mParents;

    if(formatargs[0]->mKind != Node::Kind::KIND_STR)
    {
        throw GraphBuildException("First argument to concat needs to be a string", Cell());
    }

    auto formatstr = formatargs[0]->mToken;
    std::string str;
    size_t i = 1;

    for(auto s = formatstr.begin(); s != formatstr.end(); ++s)
    {
        if(*s == '%') // print arg
        {
            if(i > formatargs.size())
            {
                std::stringstream err;
                err << "Not enough arguments for Format. Got " << formatargs.size() <<
                    " Expected " << i;
                throw GraphBuildException(err.str(), Cell());
            }

            auto arg = formatargs[i];
            if((arg->mKind != Node::Kind::KIND_STR) && (arg->mKind != Node::Kind::KIND_CONST))
            {
                std::stringstream err;
                err << "Incorrect argument " << i << " type for 'Format'."
                    " Got " << arg->mKind << ". Expected Constant or String";
                throw GraphBuildException(err.str(), Cell());
            }

            auto offset = s - formatstr.begin();
            auto after = formatstr.find_first_of(types, offset);
            char formatchr = formatstr[after];
            if((formatchr != 'f') && (formatchr != 's'))
            {
                std::stringstream err;
                err << "Only %f and %s formatters are supported";
                throw GraphBuildException(err.str(), Cell());
            }

            char buf[512];
            if(formatchr == 'f')
            {
                snprintf(buf, sizeof(buf) / sizeof(buf[0]), 
                        formatstr.substr(offset, after-offset+1).c_str(), std::stod(arg->mToken));
            } 
            else if (formatchr == 's')
            {
                snprintf(buf, sizeof(buf) / sizeof(buf[0]), 
                        formatstr.substr(offset, after-offset+1).c_str(), arg->mToken.c_str());
            }
            str += buf;
            s += after - offset;
            ++i;
        }
        else
        {
            str.append(1, *s);
        }
    }

    auto fstr = BuildNode(KIND_STR);
    fstr->mToken = str;

    return fstr;
}

bool SearchForFile(const std::string& name, std::string& out)
{
    std::stringstream buffer;
    char* searchPath = getenv(EXYS_REQUIRE_PATH);
    std::string sp = searchPath != nullptr ? std::string(searchPath) : std::string();
    std::stringstream ss(sp);
    std::string path;
    auto t = std::unique_ptr<std::ifstream>(new std::ifstream(name));
    while(!t->good() && searchPath && std::getline(ss, path, ':'))
    {
        auto t = std::unique_ptr<std::ifstream>(new std::ifstream(path+"/"+name));
    }

    if(t->good())
    {
        buffer << t->rdbuf();
        out = buffer.str();
        return true;
    }

    for(size_t i = 0; i < sizeof(StdLibEntries) / sizeof(StdLibEntries[0]); ++i)
    {
        if(StdLibEntries[i].name == name)
        {
            out = std::string((char*)StdLibEntries[i].data, StdLibEntries[i].len);
            return true;
        }
    }
    return false;
}

Node::Ptr Graph::Require(Node::Ptr node)
{
    ValidateFunctionArgs("require", node, {KIND_STR});
    const auto& fname = node->mParents[0]->mToken;
    std::string buffer;
    if(!SearchForFile(fname, buffer))
    {
        std::stringstream err;
        err << "Cannot open required file to load " << 
            node->mParents[0]->mToken << ". Check EXYS_REQUIRE_PATH variable";
        throw GraphBuildException(err.str(), Cell());
    }

    auto loadedGraph = BuildNode<Graph>(this);
    try
    {
        loadedGraph->Construct(Parse(buffer));
    }
    catch (Exys::GraphBuildException e)
    {
        e.mError = "From required file \"" + fname + "\" - "+ e.mError;
        throw e;
    }
    for(auto pcell : loadedGraph->mProvidedNodes)
    {
        auto pnode = loadedGraph->LookupSymbol(pcell);
        DefineNode(pcell.details.text, pnode);
    }
    return nullptr;
}

Node::Ptr Graph::PrintLib(Node::Ptr node)
{
    ValidateFunctionArgs("print-lib", node, {KIND_STR});
    const auto& fname = node->mParents[0]->mToken;
    std::string buffer;
    if(!SearchForFile(fname, buffer))
    {
        std::stringstream err;
        err << "Cannot open required file to load " << 
            node->mParents[0]->mToken << ". Check EXYS_REQUIRE_PATH variable";
        throw GraphBuildException(err.str(), Cell());
    }

    std::cout << buffer;

    return nullptr;
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
    AddProcFactory("apply",     WRAP(Apply));
    AddProcFactory("append",    WRAP(Append));
    AddProcFactory("nth",       WRAP(Nth));
    AddProcFactory("format",    WRAP(Format));
    AddProcFactory("require",   WRAP(Require));
    AddProcFactory("print-lib", WRAP(PrintLib));
}

Graph::Graph(std::vector<Node::Ptr> nodes)
: Node(KIND_GRAPH) 
, mParent(nullptr)
{
    mAllNodes.insert(mAllNodes.end(), nodes.begin(), nodes.end());
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
        ValidateArgsNotNull(node);
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
    assert(pnode && "Building factory returns null");
    mVarNodes[id] = pnode;
}

void Graph::SetSymbol(const Cell& cell, Node::Ptr node)
{
    assert(node);
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
        if(!niter->second)
        {
            std::stringstream err;
            err << "Symbol not available for use. Is it a valid expression - " << cell.details.text;
            throw GraphBuildException(err.str(), cell);
        }
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
    if (!node || node->mKind != Node::KIND_PROC_FACTORY)
    {
        std::stringstream err;
        err << "Not a valid procedure - " << cell.details.text;
        throw GraphBuildException(err.str(), cell);
    }
    return static_cast<ProcNodeFactory*>(node.get())->mFactory;
}

void Graph::DefineNode(const std::string& token, const Cell& exp)
{
    auto parent = Build(exp);
    assert(parent);
    mVarNodes[token] = parent;
}

void Graph::DefineNode(const std::string& token, Node::Ptr node)
{
    assert(node);
    mVarNodes[token] = node;
}

void Graph::BuildInputList(Node::Ptr child, const std::string& token, std::deque<int> dims)
{
    if(dims.size() == 0) return;

    int dim = dims.front();
    dims.pop_front();

    for(int i = 0; i < dim; i++)
    {
        auto interList = BuildNode(dims.empty() ? KIND_BIND : KIND_LIST);

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
        node->mLength = std::max(node->mLength, length);
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
                try
                {
                    Build(c);
                }
                catch (Exys::GraphBuildException e)
                {
                    e.mCell = mCurrentCell;
                    throw e;
                }
                return;
            }
        }
    }
    throw GraphBuildException("Construct not passed root cell from parser", cell);
}

Node::Ptr Graph::Build(const Cell &cell)
{
    mCurrentCell = cell;
    Node::Ptr ret = nullptr;
    if(cell.type == Cell::Type::SYMBOL)
    {
        ret = LookupSymbol(cell);
        if(ret->mKind == KIND_VAR)
        {
            auto loadVar = BuildNode(KIND_PROC);
            loadVar->mToken = "load";
            loadVar->mParents.push_back(ret);
            ret = loadVar;
        }
    }
    else if(cell.type == Cell::Type::STRING)
    {
        auto cnode = BuildNode(KIND_STR);
        cnode->mToken = cell.details.text;
        ret = cnode;
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

                // Build parents and adopt their type
                DefineNode(varToken, exp);
            }
            else if(firstElem.details.text == "defvar")
            {
                ValidateListLength(cell, 3);

                auto& varToken = cell.list[1].details.text;
                auto& exp = cell.list[2];

                // Build parents and adopt their type
                auto varNode = BuildNode(KIND_VAR);
                varNode->mToken = varToken;
                varNode->mInitValue = std::stod(exp.details.text);
                DefineNode(varToken, varNode);
            }
            else if(firstElem.details.text == "set!")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                auto& var = cell.list[1];
                auto& exp = cell.list[2];
        
                auto sym = LookupSymbol(var);
                auto parent = Build(exp);
                
                if(sym->mKind == KIND_VAR)
                {
                    // Build parents and adopt their type
                    auto storeNode = BuildNode(KIND_PROC);
                    storeNode->mToken = "store";
                    storeNode->mForceKeep = true;
                    storeNode->mParents.push_back(sym);
                    storeNode->mParents.push_back(Build(exp));
                    ValidateFunctionArgs("store", storeNode, {KIND_VAR, KIND_UNKNOWN});
                    SetSymbol(var, storeNode);
                    ret = storeNode;
                }
                else
                {
                    SetSymbol(var, parent);
                    ret = parent;
                }
            }
            else if(firstElem.details.text == "lambda")
            {
                ValidateListLength(cell, 3, 3);
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
                ValidateListLength(cell, 2, 3);
                
                std::string token = cell.list[1].details.text;
                std::string label = token;
                if(cell.list.size() == 3)
                {
                    auto inputStr = Build(cell.list[1]);
                    if(inputStr->mKind != KIND_STR)
                    {
                        throw GraphBuildException("input expected a string as the first argument", cell);
                    }
                    label = inputStr->mToken;
                    token = cell.list[2].details.text;
                }

                auto inputNode = BuildNode(KIND_BIND);
                inputNode->mToken = token;
                inputNode->mInputLabels.push_back(label);
                inputNode->mIsInput = true;
                
                if(mVarNodes.find(token) != mVarNodes.end())
                {
                    throw GraphBuildException("Input \"" + token + "\" overrides previously defined token", cell);
                }

                DefineNode(token, inputNode);
            }
            else if(firstElem.details.text == "input-list")
            {
                ValidateListLength(cell, 3);

                // check input hasn't already been declared
                auto& inputToken  = cell.list[1].details.text;
                auto inputList = BuildNode(KIND_LIST);
                inputList->mToken = inputToken;

                DefineNode(inputToken, inputList);

                // Add dimensions
                std::deque<int> dims;
                uint16_t length=1;
                for(auto iter = cell.list.begin()+2;
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
            else if(firstElem.details.text == "observe")
            {
                ValidateListLength(cell, 2, 3);

                // Add check that token already exists
                // Add check that we aren't already ouputing to this observer
                auto varNode = Build(cell.list[1]);
                std::string token = cell.list[1].details.text;
                switch (varNode->mKind)
                {
                    case KIND_UNKNOWN:
                    case KIND_PROC_FACTORY:
                    case KIND_GRAPH:
                        throw GraphBuildException("Anonymous node isn't observerable", cell);
                    case KIND_PROC:
                    case KIND_CONST:
                    case KIND_BIND:
                    case KIND_VAR:
                    case KIND_STR:
                    case KIND_LIST:
                        break;
                }

                if(cell.list.size() == 3)
                {
                    if(varNode->mKind != KIND_STR)
                    {
                        throw GraphBuildException("First arg must be string", cell);
                    }
                    token = varNode->mToken;
                    varNode = Build(cell.list[2]);
                }

                if(!varNode)
                {
                    throw GraphBuildException("Node isn't observerable", cell);
                }

                // Register Observer
                LabelObserver(varNode, token);
                LabelListRoot(varNode, token, GetListLength(varNode), false);
                varNode->mIsObserver = true;
            }
            else if(firstElem.details.text == "provide")
            {
                ValidateListLength(cell, 2, 2);
                mProvidedNodes.push_back(cell.list[1]);
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
    auto nodeLayout = GetLayout();

    int i = 0;
    std::string ret = "{\n";
    for(auto& nodeptr : nodeLayout)
    {
        auto childLabel = NodeToPtrString(nodeptr);
        for(auto parent : nodeptr->mParents)
        {
            ret += NodeToPtrString(parent) + " -> " 
                + childLabel + "\n";
        }
        if(nodeptr->mIsObserver)
        {
            for(const auto& label : nodeptr->mObserverLabels)
            {
                ret += childLabel + " -> " 
                    + std::to_string(i) + "\n" 
                    + std::to_string(i) + " [label=\"" + label
                    + "\"]\n";
            }
            ++i;
        }
        ret += NodeToDotLabel(nodeptr) + "\n";
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

void Graph::CollectForceKeep(Node::Ptr node, std::vector<Node::Ptr>& nodes) const
{
    if(node->mKind == Node::KIND_GRAPH)
    {
        auto g = static_cast<Graph*>(node.get());
        for(auto n : g->mAllNodes)
        {
            CollectForceKeep(n, nodes);
        }
    }
    else if(node->mForceKeep)
    {
        CollectListMembers(node, nodes);
    }
}

void Graph::CollectInputs(Node::Ptr node, std::vector<Node::Ptr>& inputs) const
{
    if(node->mKind == Node::KIND_GRAPH)
    {
        auto g = static_cast<Graph*>(node.get());
        for(auto n : g->mAllNodes)
        {
            CollectInputs(n, inputs);
        }
    }
    else if(node->mIsInput)
    {
        CollectListMembers(node, inputs);
    }
}

void Graph::CollectObservers(Node::Ptr node, std::vector<std::vector<Node::Ptr>>& observers) const
{
    if(node->mKind == Node::KIND_GRAPH)
    {
        auto g = static_cast<Graph*>(node.get());
        for(auto n : g->mAllNodes)
        {
            CollectObservers(n, observers);
        }
    }
    else if(node->mIsObserver)
    {
        std::vector<Node::Ptr> nodes;
        CollectListMembers(node, nodes);
        observers.push_back(nodes);
    }
}

// We try not to update any properties of the nodes
// in this function but if we do it has to be repeatable 
// re. offsets
std::vector<Node::Ptr> Graph::GetLayout() const
{
    std::vector<Node::Ptr> layout;

    // Find end points of DAG - observers and forcekeeps
    std::vector<std::vector<Node::Ptr>> observers;
    std::vector<Node::Ptr> forceKeep;
    std::vector<Node::Ptr> inputs;
    for(const auto an : mAllNodes)
    {
        CollectInputs(an, inputs);
        CollectForceKeep(an, forceKeep);
        CollectObservers(an, observers);
    }
    
    // Find the necessary nodes - nodes connected to an observer or forcekeep
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
    for(auto fk : forceKeep)
    {
        uint64_t height=1;
        TraverseNodes(fk, height, necessaryNodes);
    }

    // Step 1 - Add inputs to layout
    uint64_t inputOffset = 0;
    for(auto in : inputs)
    {
        in->mIsInput = true;
        in->mInputOffset = inputOffset++;
        layout.push_back(in);
    }

    // Step 2 - Remove inputs from necessary nodes
    for(auto n : layout)
    {
        necessaryNodes.erase(n);
    }
    
    // Step 3 - Add necessary nodes
    for(auto n : necessaryNodes)
    {
        layout.push_back(n);
    }

    // Step 4 - Add observer offset and if list or observing input add copies
    uint64_t observerOffset = 0;
    for(auto oi : observers)
    {
        for(auto node : oi)
        {
            if((oi.size() > 1) || node->mIsInput)
            {
                auto nodeCopy = std::make_shared<Node>(Node::KIND_PROC);
                nodeCopy->mHeight = 0;
                nodeCopy->mToken = "copy";
                nodeCopy->mIsObserver = true;
                nodeCopy->mObserverLabels = node->mObserverLabels;
                nodeCopy->mInputLabels = node->mInputLabels;
                nodeCopy->mLength = node->mLength;
                nodeCopy->mObserverOffset = observerOffset++;
                nodeCopy->mParents.push_back(node);
                layout.push_back(nodeCopy);
            }
            else
            {
                node->mObserverOffset = observerOffset++;
            }
        }
    }
    return layout;
}

std::vector<std::unique_ptr<Graph>> Graph::SplitOutBy(Node::Kind kind, const std::string& token)
{
    std::vector<std::unique_ptr<Graph>> graphs;

    std::vector<Node::Ptr> inputs;
    for(auto an : mAllNodes)
    {
        CollectInputs(an, inputs);
    }

    for(auto n : mAllNodes)
    {
        if((n->mKind == Node::KIND_PROC) && (n->mToken.compare(token) == 0))
        {
            std::vector<Node::Ptr> graphNodes;
            graphNodes.insert(graphNodes.end(), inputs.begin(), inputs.end());
            graphNodes.push_back(n);

            graphs.emplace_back(new Graph(graphNodes));
        }
    }
    return graphs;
}

// This is kinda nasty post processing step but probably the best way
// to go for a couple of reasons. We also assume all nodes are necessary
// 1. Backend stages cannot handle lists - they need operations between two
// doubles
// 2. Graph don't have concept of feedback so we cant overwrite inputs
// 3. Alot of information used about param layout is implicitly contained
// in the GetLayout function which is fine when its abstracted away from users
// but we are trying to abuse that here
std::vector<Node::Ptr> Graph::GetSimApplyLayout() const
{ 
    // Flatten collected nodes into continous block
    // Step 1 - Add inputs
    auto nodeLayout = GetLayout();
    int64_t maxOffset = 0;
    for(auto an : nodeLayout)
    {
        maxOffset = std::max(an->mInputOffset, maxOffset);
    }

    std::vector<Node::Ptr> simnodes;
    for(auto n : mAllNodes)
    {
        if((n->mKind == Node::KIND_PROC) && (n->mToken.compare("sim-apply") == 0))
        {
            simnodes.push_back(n);
        }
    }

    assert(simnodes.size()==1 && "Split out simapplys into own graph before getting the layout");
    
    std::vector<Node::Ptr> expandedSimApply;
    // Expand sim apply
    {
        auto n = simnodes[0];
        auto args = n->mParents;
        auto target = std::static_pointer_cast<Node>(args[0]);
        auto overwrite = std::static_pointer_cast<Node>(args[1]);
        auto doneFlag = std::static_pointer_cast<Node>(args[2]);

        doneFlag->mIsObserver = true;
        doneFlag->mHeight = 0;
        doneFlag->mToken = doneFlag->mToken;
        doneFlag->mInputLabels.push_back("sim-done");
        doneFlag->mObserverOffset = maxOffset+1; // Want it to be last
        expandedSimApply.push_back(doneFlag);

        if(target->mKind == KIND_LIST)
        {
            std::vector<Node::Ptr> simListNodes;
            CollectListMembers(target, simListNodes);

            std::vector<Node::Ptr> overwriteNodes;
            CollectListMembers(overwrite, overwriteNodes);

            assert(simListNodes.size() == overwriteNodes.size() && 
                "Sim output to list doesn't match target");

            for(size_t i = 0; i < simListNodes.size(); ++i)
            {
                auto simapply = std::make_shared<Node>(Node::KIND_PROC);
                simapply->mIsObserver = true;
                simapply->mHeight = 0;
                simapply->mToken = "sim-apply";
                simapply->mObserverLabels = simListNodes[i]->mInputLabels;
                simapply->mObserverOffset = simListNodes[i]->mInputOffset;
                simapply->mLength = 1;
                simapply->mParents.push_back(overwriteNodes[i]);
                expandedSimApply.push_back(simapply);
            }
        }
        else
        {
            n->mIsObserver = true;
            n->mHeight = 0;
            n->mToken = "sim-apply";
            n->mObserverLabels = target->mInputLabels;
            n->mObserverOffset = target->mInputOffset;
            n->mLength = 1;
            n->mParents.clear();
            n->mParents.push_back(overwrite);
            expandedSimApply.push_back(n);
        }
    }

    // Collect necessary nodes and set heights
    std::set<Node::Ptr> necessarySimNodes;
    for(auto n : expandedSimApply)
    {
        uint64_t height=1;
        TraverseNodes(n, height, necessarySimNodes); 
    }
    
    // Build layout
    std::vector<Node::Ptr> layout;
    for(auto n : necessarySimNodes)
    {
        layout.push_back(n);
    }

    return layout;
}

std::string Graph::GetSimApplyTarget() const
{
    std::vector<Node::Ptr> simnodes;
    for(auto n : mAllNodes)
    {
        if((n->mKind == Node::KIND_PROC) && (n->mToken.compare("sim-apply") == 0))
        {
            simnodes.push_back(n);
        }
    }

    assert(simnodes.size()==1 && "Split out simapplys into own graph before getting the layout");
    return std::static_pointer_cast<Node>(simnodes[0]->mParents[0])->mToken;
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
