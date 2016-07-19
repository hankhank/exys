
#include <exys.h>

#include <cctype>
#include <cwctype>

// debug headers
#include <iostream>

namespace Exys
{

// convert given string to list of tokens
std::list<std::string> Tokenize(const std::string & str)
{
    std::list<std::string> tokens;
    const char * s = str.c_str();
    while (*s) 
    {
        while (iswspace(*s)) ++s;
        if (*s == '(' || *s == ')')
        {
            tokens.push_back(*s++ == '(' ? "(" : ")");
        }
        else 
        {
            const char * t = s;
            while (*t && *t != ' ' && *t != '(' && *t != ')')
            {
                ++t;
            }
            tokens.push_back(std::string(s, t));
            s = t;
        }
    }
    return tokens;
}

// return the Lisp expression in the given tokens
Cell ReadFromTokens(std::list<std::string>& tokens)
{
    const std::string token(tokens.front());
    tokens.pop_front();
    if (token == "(") 
    {
        auto l = Cell::List();
        while (tokens.front() != ")")
        {
            l.list.push_back(ReadFromTokens(tokens));
        }
        tokens.pop_front();
        return l;
    }
    else
    {
        return Cell::Symbol(token);
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

Node::Ptr Graph::LookupSymbol(const std::string token)
{
    auto niter = std::find(mVarNodes.begin(), mVarNodes.end(), token);
    if (niter == mVarNodes.end())
    {
        // raise
    }
    return *niter;
}

void Graph::SetMostRecentVarNode(const std::string varToken, Node::Ptr varNode)
{
}

Node::Ptr Graph::Build(const Cell &cell, Node::Ptr childNode)
{
    if(cell.type == Cell::Type::SYMBOL)
    {
        return LookupSymbol(cell.token);
    }
    if(cell.type == Cell::Type::NUMBER)
    {
        auto constNode = std::make_shared<ConstNode>();
        return constNode;
    }
    else if(cell.type == Cell::Type::LIST)
    {
        if (cell.list.empty())
        {
            return nullptr;
        }

        auto& firstElem = cell.list.front();
        if(firstElem.token == "?")
        {
            // Add check for list length
            auto& condExp = cell.list[1];
            auto& posExp  = cell.list[2];
            auto& negExp  = cell.list[3];
            
            auto ifNode = BuildIfNode(varNode);
            auto condNode = Build(condExp, ifNode);
            auto posNode = Build(posExp, ifNode);
            auto negNode = Build(negExp, ifNode);
            ifNode->setConditionalNode(condNode);
            return ifNode;
        }
        else if(firstElem.token == "define")
        {
            // Add check for list length
            auto& varToken = cell.list[1].token;
            auto& exp = cell.list[2];

            // check whether its been defined in this scope
                
            // Create Node
            auto varNode = std::make_shared<Node>();
            varNode->kind = Node::Kind::VAR;
            varNode->type = Node::Type::UNKNOWN;
            varNode->token = varToken;
            varNode->recomputeId = mStabilisationId;
            varNode->changeId = mStabilisationId;
            varNode->height = 0;
            varNode->value.i = 0;

            mVarNodes[varToken] = varNode;
            
            // Build parents and adopt their type
            auto parent = Build(exp, varNode);
            varNode->type = parent->type;
            
            return nullptr;
        }
        else if(firstElem.token == "set!")
        {
            // Add check for list length
            // Add check that token already exists
            
            auto& varToken = cell.list[1].token;
            auto& exp = cell.list[2];

            // Create Node
            auto varNode = std::make_shared<Node>();
            varNode->kind = Node::Kind::VAR;
            varNode->type = Node::Type::UNKNOWN;
            varNode->token = varToken;
            varNode->recomputeId = mStabilisationId;
            varNode->changeId = mStabilisationId;
            varNode->height = 0;
            varNode->value.i = 0;
            
            // Build parents and adopt their type
            auto parent = Build(exp, varNode);
            varNode->type = parent->type;

            SetMostRecentVarNode(varToken, varNode);

            return nullptr;
        }
        else if(firstElem.token == "lambda")
        {
            // Think I need to capture tree here
            // and build graph whenever invoked
            // I think this will be the same for
            // all procs
            // For map
        }
        else if(firstElem.token == "input")
        {
            // Add check for list length
            // Add check whether token in list send warn
            auto& inputTypeToken = cell.list[1].token;

            // Check valid type
            auto inputType = InputType2Enum(inputType);
    
            // Add input node
            for(auto& inputToken : cell.at(2))
            {
                auto& inputToken = cell.list[2].token;

                // check input hasn't already been declared
                
                auto inputNode = std::make_shared<Node>();
                inputNode->kind = Node::Kind::INPUT;
                inputNode->type = inputType;
                inputNode->token = inputToken;
                inputNode->recomputeId = mStabilisationId;
                inputNode->changeId = mStabilisationId;
                inputNode->height = 0;
                inputNode->value.i = 0;

                mVarNodes[varToken] = varNode;
            }
        }
        else if(firstElem.token == "observe")
        {
            // Add check for list length
            // Add check that token already exists
            auto& varToken = cell.list[1].token;
            auto& ouputToken = cell.list[2].token;
            auto varNode = LookupSymbol(cell.token);

            // Register Observer
            observers.push_back(varNode);
        }
        else // procedure call
        {
        }
    }
}

Graph BuildGraph(const std::vector<Cell>& procs)
{
    Graph graph;
    for (auto& proc : procs)
    {
        graph.Eval(proc);
    }
    graph.Complete();
}

}

int main()
{
    const auto& d = std::string("(input Books)\n(define res (+ 0 Books.bids.level0.price Books.asks.level1.price))\n(set! res (+ res 1))\n(output res Double.val)");
    
    auto procs = Exys::Parse(d);
    auto graph = Exys::BuildGraph(procs);
    return 0;
}

//void Vwap::Recv(GwdSignal& signal)
//{
//    switch (signal.connection.signalId)
//    {
//    };
//}
//
