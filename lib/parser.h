#pragma once

#include <vector>
#include <string>

namespace Exys
{

struct Cell
{
    enum Type 
    {
        LIST,
        SYMBOL,
        NUMBER
    };

    Type type;
    std::string token;
    int64_t offset;
    std::vector<Cell> list;
    
    static Cell Symbol(const std::string& tok, int offset) 
    { return {SYMBOL, tok, offset, {}};}
    static Cell Number(const std::string& tok, int offset) 
    { return {NUMBER, tok, offset, {}};}
    static Cell List(int offset) 
    { return {LIST, "", offset, {}};}
};

Cell Parse(const std::string& val);

}
