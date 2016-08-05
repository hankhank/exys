#include <cctype>
#include <cwctype>
#include <cassert>
#include <algorithm>
#include <stack>

#include "parser.h"

namespace Exys
{

// convert given string to list of tokens
std::list<TokenDetails> Tokenize(const std::string & str)
{
    std::list<TokenDetails> tokens;

    bool inComment = false;

    std::string tok;
    int lineNumber = 0;
    int column = 0;
    int startLineNumber = 0;
    int startColumn = 0;

    auto trypushtoken = [&]()
    {
        if(tok.size())
        {
            tokens.push_back({tok, startLineNumber, startColumn, lineNumber, startColumn+tok.size()-1});
            tok.clear();
        }
    };

    for(auto s = str.begin(); s != str.end(); ++s)
    {
        if(*s == ';')
        {
            inComment = true;
        }
        else if(*s == '\n')
        {
            trypushtoken();
            inComment = false;
            ++lineNumber;
            column = 0;
            continue;
        }
        else if(inComment)
        {
            trypushtoken();
        }
        else if(*s == '(')
        {
            trypushtoken();
            tok = "(";
            startLineNumber = lineNumber;
            startColumn = column;
            trypushtoken();   
        }
        else if(*s == ')')
        {
            trypushtoken();
            tok = ")";
            startLineNumber = lineNumber;
            startColumn = column;
            trypushtoken();   
        }
        else if (iswspace(*s))
        {
            trypushtoken();   
        }
        else
        {
            if(!tok.size())
            {
                startLineNumber = lineNumber;   
                startColumn = column;   
            }
            tok.append(1, *s);
        }
        ++column;
    }

    return tokens;
}

Cell Atom(TokenDetails token)
{
    if
    (
        (token.text.size() && std::isdigit(token.text[0])) || 
        (token.text.size() > 1 && token.text[0] == '-' && std::isdigit(token.text[1]))
    )
    {
        return Cell::Number(token);
    }
    return Cell::Symbol(token);
}

// return the Lisp expression in the given tokens
Cell ReadFromTokenDetails(const std::list<TokenDetails>& tokens)
{
    Cell root;
    if(!tokens.size())
    {
        // nothing
        return root;
    }

    std::stack<Cell*> cellStack;
    cellStack.push(&root);

    for(auto token : tokens)
    {
        auto* cell = cellStack.top();
        if(token.text == "(")
        {
            cell->list.emplace_back(Cell::List(token));
            cellStack.push(&cell->list.back());
        }
        else if(token.text == ")")
        {
            if(cell->type != Cell::Type::LIST)
            {
                assert(false);
                //raise;
            }
            cellStack.pop();
        }
        else
        {
            cell->list.emplace_back(Atom(token));
        }
    }
    return root.list.front();
}

Cell Parse(const std::string& val)
{
    auto tokens = Tokenize(val);
    return ReadFromTokenDetails(tokens);
}

}
