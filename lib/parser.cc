#include <list>
#include <cctype>
#include <cwctype>
#include <algorithm>

#include "parser.h"

namespace Exys
{

struct TokenDetails
{
    std::string text;
    int64_t offset;
};

// convert given string to list of tokens
std::list<TokenDetails> TokenDetailsize(const std::string & str)
{
    std::list<TokenDetails> tokens;
    const char * s = str.c_str();
    const char * ref = s;
    while (*s) 
    {
        if (*s == ';')
        {
            while(*s && *s++ != '\n');
            continue;
        }
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
            if(s != t)
            {
                auto tokstr = std::string(s,t);
                tokstr.erase(std::remove(tokstr.begin(), tokstr.end(), '\n'), tokstr.end());
                tokens.push_back({tokstr, s-ref});
            }
            s = t;
        }
    }
    return tokens;
}

Cell Atom(TokenDetails token)
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
Cell ReadFromTokenDetails(std::list<TokenDetails>& tokens)
{
    auto token = tokens.front();
    tokens.pop_front();
    if (token.text == "(") 
    {
        auto l = Cell::List(token.offset);
        while (tokens.front().text != ")")
        {
            l.list.push_back(ReadFromTokenDetails(tokens));
        }
        tokens.pop_front();
        return l;
    }
    else
    {
        return Atom(token);
    }
}

Cell Parse(const std::string& val)
{
    auto tokens = TokenDetailsize(val);
    return ReadFromTokenDetails(tokens);
}

}
