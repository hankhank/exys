#pragma once

#include <vector>
#include <string>
#include <list>

namespace Exys
{

struct TokenDetails
{
    std::string text;
    int firstLineNumber;
    int firstColumn;
    int endLineNumber;
    int endColumn;
};

struct Cell
{
    enum Type 
    {
        NONE,
        ROOT,
        LIST,
        SYMBOL,
        NUMBER
    };
    
    Type type = NONE;
    TokenDetails details;

    std::vector<Cell> list;

    static Cell Root() 
    {
        Cell c;
        c.type = ROOT;
        return c;
    }

    static Cell Symbol(const TokenDetails& details) 
    {
        Cell c;
        c.type = SYMBOL;
        c.details = details;
        return c;
    }

    static Cell Number(const TokenDetails& details) 
    {
        Cell c;
        c.type = NUMBER;
        c.details = details;
        return c;
    }

    static Cell List(const TokenDetails& details) 
    {
        Cell c;
        c.type = LIST;
        c.details = details;
        return c;
    }
};

class ParseException : public std::exception
{
public:
    ParseException(const std::string& error, TokenDetails details);

    virtual const char* what() const noexcept(true);
    std::string GetErrorMessage(const std::string& inputText) const;

    std::string mError;
    TokenDetails mDetails;
};

std::list<TokenDetails> Tokenize(const std::string& str);
Cell Atom(TokenDetails token);
Cell ReadFromTokenDetails(const std::list<TokenDetails>& tokens);

Cell Parse(const std::string& val);

}
