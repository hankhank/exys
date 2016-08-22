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
    union
    {
        bool mB;
        int mI;
        unsigned int mU;
        double mD;
    }

    bool operator!=(const Signal& rhs)
    {
        return mD != rhs.d;
    }

    Point& operator=(bool b) {mB = b;}
    Point& operator=(int i)  {mI = i;}
    Point& operator=(unsigned int u) {mU = u;}
    Point& operator=(double d) {mD = d;}

    bool operator==(bool b) {return mB = b;}
    bool operator==(int i)  {return mI = i;}
    bool operator==(unsigned int u) {return mU = u;}
    bool operator==(double d) {return mD = d;}

    bool operator!=(bool b) {return mB != b;}
    bool operator!=(int i)  {return mI != i;}
    bool operator!=(unsigned int u) {return mU != u;}
    bool operator!=(double d) {return mD != d;}
};

class IEngine
{
public:
    virtual ~IEngine() {}

    virtual void PointChanged(Point& point) = 0;
    virtual void Stabilize() = 0;
    virtual bool IsDirty() = 0;

    virtual bool HasInputPoint(const std::string& label) = 0;
    virtual Point& LookupInputPoint(const std::string& label) = 0;
    virtual std::vector<std::string> GetInputPointLabels() = 0;
    virtual std::unordered_map<std::string, double> DumpInputs() = 0;

    virtual bool HasObserverPoint(const std::string& label) = 0
    virtual Point& LookupObserverPoint(const std::string& label) = 0;
    virtual std::vector<std::string> GetObserverPointLabels() = 0;
    virtual std::unordered_map<std::string, double> DumpObservers() = 0;

    virtual std::string GetDOTGraph() = 0;

    static std::unique_ptr<IEngine> Build(const std::string& text);

};

};
