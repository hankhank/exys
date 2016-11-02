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


struct Point
{
    static constexpr double POINT_EPSILON = 0.000001;
    double mVal = 0.0;
    bool mDirty = false;
    uint16_t mLength=1;

    bool operator!=(const Point& rhs)
    {
        return std::abs(mVal - rhs.mVal) > POINT_EPSILON;
    }

    virtual Point& operator[](size_t i)
    {
        assert(i < mLength);
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

class IEngine
{
public:
    virtual ~IEngine() {}

    virtual void Stabilize() = 0;
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
