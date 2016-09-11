#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <set>

#include "graph.h"

namespace Exys
{

struct Point
{
    double mVal = 0.0;
    bool mDirty = false;

    bool operator!=(const Point& rhs)
    {
        return mVal != rhs.mVal;
    }

    Point& operator=(Point& p) {mDirty = mVal != p.mVal; mVal = p.mVal;  return *this;}
    Point& operator=(double d) {mDirty = mVal != d; mVal = d; return *this;}
    bool operator==(double d)  {return mVal = d;}
    bool operator!=(double d)  {return mVal != d;}
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
