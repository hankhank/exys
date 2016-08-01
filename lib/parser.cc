#include <cctype>
#include <cwctype>
#include <algorithm>

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

    auto pushtoken = [&]()
    {
        tokens.push_back({tok, startLineNumber, startColumn, lineNumber, column});
        tok.clear();
    };

    for(auto s = str.begin(); s != str.end(); ++s)
    {
        if(*s == ';')
        {
            inComment = true;
        }
        else if(*s == '\n')
        {
            inComment = false;
            ++lineNumber;
            column = 0;
        }
        else if(*s == '(')
        {
            if(tok.size())
            {
                pushtoken();
            }
            std::string tok = "(";
            startLineNumber = lineNumber;
            startColumn = column;
            pushtoken();   
        }
        else if(*s == ')')
        {
            if(tok.size())
            {
                pushtoken();   
            }
            std::string tok = ")";
            startLineNumber = lineNumber;
            startColumn = column;
            pushtoken();   
        }
        else if (iswspace(*s))
        {
            if(tok.size())
            {
                pushtoken();   
            }
        }
        else
        {
            if(!tok.size())
            {
                startLineNumber = lineNumber;   
                startColumn = column;   
            }
            tok.append(&*s);
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
Cell ReadFromTokenDetails(std::list<TokenDetails>& tokens)
{
    auto token = tokens.front();
    tokens.pop_front();
    if (token.text == "(") 
    {
        auto l = Cell::List(token);
        if (!tokens.empty())
        {
            while (tokens.front().text != ")")
            {
                l.list.push_back(ReadFromTokenDetails(tokens));
            }
            tokens.pop_front();
        }
        return l;
    }
    else
    {
        return Atom(token);
    }
}

Cell Parse(const std::string& val)
{
    auto tokens = Tokenize(val);
    return ReadFromTokenDetails(tokens);
}

}
