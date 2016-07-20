
#include <exys.h>

#include <cctype>
#include <cwctype>

// debug headers
#include <iostream>
#include <sstream>

namespace Exys
{

struct Token
{
    std::string text;
    int64_t offset;
};

// convert given string to list of tokens
std::list<Token> Tokenize(const std::string & str)
{
    std::list<Token> tokens;
    const char * s = str.c_str();
    const char * ref = s;
    while (*s) 
    {
        while (iswspace(*s)) ++s;
        if (*s == '(' || *s == ')')
        {
            tokens.push_back({*s++ == '(' ? "(" : ")", s-ref});
        }
        else 
        {
            const char * t = s;
            while (*t && *t != ' ' && *t != '(' && *t != ')')
            {
                ++t;
            }
            tokens.push_back({std::string(s, t), s-ref});
            s = t;
        }
    }
    return tokens;
}

Cell Atom(Token token)
{
    auto text = token.text;
    auto offset = token.offset;
    if
    (
        (text.size() && std::isdigit(text[0])) || 
        (text.size() > 1 && text[0] == '-' && std::isdigit(text[1]))
    )
    {
        return Cell::Number(text, offset);
    }
    return Cell::Symbol(text, offset);
}

// return the Lisp expression in the given tokens
Cell ReadFromTokens(std::list<Token>& tokens)
{
    auto token = tokens.front();
    tokens.pop_front();
    if (token.text == "(") 
    {
        auto l = Cell::List(token.offset);
        while (tokens.front().text != ")")
        {
            l.list.push_back(ReadFromTokens(tokens));
        }
        tokens.pop_front();
        return l;
    }
    else
    {
        return Atom(token);
    }
}

std::vector<Cell> Parse(const std::string& val)
{
    std::vector<Cell> procedures;
    auto tokens = Tokenize(val);
    while(tokens.size())
    {
        procedures.push_back(ReadFromTokens(tokens));
    }
    return procedures;
}

template<typename T>
Node::Ptr Graph::BuildNode(const std::vector<Cell>& args)
{
    auto mn = std::make_shared<T>();
    for(auto& arg : args)
    {
        auto leg = Build(arg);
        mn->mParents.push_back(leg);
    }
    return mn;
}

#define ADD_PROC(__ID, __BUILDER) \
    mProcs[std::string(__ID)] = [this](std::vector<Cell> args)->Node::Ptr \
                        {return this->__BUILDER(args);}

Graph::Graph()
{
    ADD_PROC("+", BuildNode<SumNode>);
    ADD_PROC("-", BuildNode<SubNode>);
    ADD_PROC("/", BuildNode<DivNode>);
    ADD_PROC("*", BuildNode<MulNode>);
}

Node::Ptr Graph::LookupSymbol(const Cell& cell)
{
    auto niter = mVarNodes.find(cell.token);
    if (niter == mVarNodes.end())
    {
        std::stringstream err;
        err << "Could not find symbol - " << cell.token;
        throw GraphBuildException(err.str(), cell);
    }
    return niter->second;
}

Graph::GraphFactory Graph::LookupProcedure(const Cell& cell)
{
    auto niter = mProcs.find(cell.token);
    if (niter == mProcs.end())
    {
        std::stringstream err;
        err << "Could not find procedure - " << cell.token;
        throw GraphBuildException(err.str(), cell);
    }
    return niter->second;
}

Node::Type Graph::InputType2Enum(const std::string& token)
{
    return Node::Type::TYPE_DOUBLE;
}

void ValidateListLength(const Cell& cell, size_t min)
{
    if(cell.list.size() < min)
    {
        std::string error;
        std::stringstream err(error);
        err << "List length too small. Expected at least "
            << min << " Got " << cell.list.size();
        throw GraphBuildException(error, cell);
    }
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
        ret = std::make_shared<ConstNode>();
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
                
                auto ifNode = std::make_shared<TenaryNode>();
                auto condNode = Build(condExp);
                auto posNode = Build(posExp);
                auto negNode = Build(negExp);

                // Check neg and pos legs have same type
                ifNode->SetLegs(condNode, posNode, negNode);

                return ifNode;
            }
            else if(firstElem.token == "define")
            {
                ValidateListLength(cell, 3);

                auto& varToken = cell.list[1].token;
                auto& exp = cell.list[2];

                // check whether its been defined in this scope

                // Build parents and adopt their type
                auto parent = Build(exp);
                mVarNodes[varToken] = parent;
            }
            else if(firstElem.token == "set!")
            {
                ValidateListLength(cell, 3);

                // Add check that token already exists
                auto& varToken = cell.list[1].token;
                auto& exp = cell.list[2];

                // Build parents and adopt their type
                auto parent = Build(exp);
                mVarNodes[varToken] = parent;
            }
            else if(firstElem.token == "lambda")
            {
                // Think I need to capture tree here
                // and build graph whenever invoked
                // I think this will be the same for
                // all procs
                // For map
 
                // Add check for list length
                // Add check whether token in list send warn
                auto& lambdaToken = cell.list[1].token;
                //mProcs[lambdaToken] = []
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
                    
                    auto inputNode = std::make_shared<InputNode>();
                    inputNode->mType = inputType;
                    inputNode->mToken = inputToken;

                    mVarNodes[inputToken] = inputNode;
                    mInputsNodes[inputToken] = inputNode;
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
                observers[outputToken] = varNode;
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

std::unique_ptr<Graph> Graph::BuildGraph(const std::string& text)
{
    auto cells = Parse(text);
    auto graph = std::make_unique<Graph>();
    for (auto& cell : cells)
    {
        graph->Build(cell);
    }
    //graph.Complete();
    return graph;
}

}

