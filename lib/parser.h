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
        LIST,
        SYMBOL,
        NUMBER
    };
    
    Type type = NONE;
    TokenDetails details;

    std::vector<Cell> list;

    static Cell Symbol(const TokenDetails& details) 
    { return {SYMBOL, details, {}}; }

    static Cell Number(const TokenDetails& details) 
    { return {NUMBER, details, {}}; }

    static Cell List(const TokenDetails& details) 
    { return {LIST, details, {}}; }
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
