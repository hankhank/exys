#pragma once

#include <string>
#include <sstream>
#include "graph.h"

namespace Exys
{

inline std::string GetLine(const std::string& text, int line)
{
    int lineNumber = 0;
    std::string::size_type pos = 0, prev = 0;
    while ((pos = text.find('\n', prev)) != std::string::npos)
    {
        if(++lineNumber > line) break;
        prev = pos + 1;
    }
    return text.substr(prev, pos == std::string::npos ? std::string::npos : pos - prev);
}

inline void DummyValidator(Node::Ptr)
{
}

inline void CheckKindForPrimitive(Node::Ptr point)
{
    Cell cell;
    std::stringstream err;
    err << "Incorrect type for primitive operation. "
        << "Needs to be constant or double. Got ";
    int i = 0;
    for(const auto p : point->mParents)
    {
        assert(p && "Attempting to check null parent");
        switch(p->mKind)
        {
            case Node::KIND_CONST: 
            case Node::KIND_BIND: 
            case Node::KIND_PROC: 
                break; // expected

            default:
            case Node::KIND_LIST: 
            case Node::KIND_PROC_FACTORY: 
            case Node::KIND_GRAPH: 
            case Node::KIND_UNKNOWN: 
            {
                err << p->mKind << " for argument " << i;
                throw GraphBuildException(err.str(), cell);
            }
            break;
        }
        ++i;
    }
}

template<size_t N>
inline void MinCountValueValidator(Node::Ptr point)
{
    if(point->mParents.size() < N)
    {
        Cell cell;
        std::stringstream err;
        err << "Not enough items in list for function. Expected at least "
            << N << " Got " << point->mParents.size();
        throw GraphBuildException(err.str(), cell);
    }
    CheckKindForPrimitive(point);
}

template<size_t N1, size_t N2>
inline void CountValueValidator(Node::Ptr point)
{
    if(point->mParents.size() < N1)
    {
        Cell cell;
        std::stringstream err;
        err << "Not enough items in list for function. Expected at least "
            << N1 << " Got " << point->mParents.size();
        throw GraphBuildException(err.str(), cell);
    }

    if(point->mParents.size() > N2)
    {
        Cell cell;
        std::stringstream err;
        err << "Too many items in list for function. Expected at most "
            << N2 << " Got " << point->mParents.size();
        throw GraphBuildException(err.str(), cell);
    }
    CheckKindForPrimitive(point);
}

inline void ValidateArgsNotNull(Node::Ptr point)
{
    int i = 0;
    for(auto n : point->mParents)
    {
        if(!n)
        {
            Cell cell;
            std::stringstream err;
            err << "Argument " << i << " is null";
            throw GraphBuildException(err.str(), cell);
        }
        ++i;
    }
}

}
