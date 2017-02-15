#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <cassert>
#include <cmath>

#include "graph.h"

namespace Exys
{

// Size and layout of this structure is very important
// as we describe it to the llvm compiler later so
// the functions we generate can operate on pointers
// of this type. Trying also to keep it to 16 bytes 
// so we can fit 4 per cache line
#pragma pack(push)
#pragma pack(1)
struct Point
{
    static constexpr double POINT_EPSILON = 0.000001;
    double mVal = 0.0;   // 8
    uint32_t mLength=1;  // 4
    bool mDirty = false; // 1
    char pad[3];         // 3

    bool IsDirty() const { return mDirty; };
    void Clean() { mDirty = false; };

    bool operator!=(const Point& rhs)
    {
        return std::abs(mVal - rhs.mVal) > POINT_EPSILON;
    }

    Point& operator[](size_t i)
    {
        assert(i < mLength);
        i = i < mLength ? i : 0;
        return *(this+i);
    }

    Point& operator=(const Point& p) 
    {
        mDirty = (mDirty || (mVal != p.mVal));
        mVal = p.mVal;
        return *this;
    }

    Point& operator=(double d)       
    {
        mDirty = (mDirty || (mVal != d));
        mVal = d;
        return *this;
    }
    bool operator==(double d) const  {return mVal == d;}
    bool operator!=(double d) const  {return mVal != d;}
};
#pragma pack(pop)

class IEngine
{
public:
    virtual ~IEngine() {}

    virtual void Stabilize(bool force=false) = 0;
    virtual bool IsDirty() = 0;

    virtual bool HasInputPoint(const std::string& label) = 0;
    virtual Point& LookupInputPoint(const std::string& label) = 0;
    virtual std::vector<std::string> GetInputPointLabels() = 0;
    virtual std::unordered_map<std::string, double> DumpInputs() = 0;

    virtual bool HasObserverPoint(const std::string& label) = 0;
    virtual Point& LookupObserverPoint(const std::string& label) = 0;
    virtual std::vector<std::string> GetObserverPointLabels() = 0;
    virtual std::unordered_map<std::string, double> DumpObservers() = 0;

    virtual std::string GetDOTGraph() = 0;
};

};
