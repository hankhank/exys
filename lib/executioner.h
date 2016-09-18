#pragma once

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

namespace Exys
{

inline std::vector<std::string> SplitLine(const std::string& text)
{
    std::vector<std::string> ret;
    std::string tok;
    for(auto s = text.begin(); s != text.end(); ++s)
    {
        if(!iswspace(*s))
        {
            tok.append(1, *s);
        }
        else
        {
            if(tok.size())
            {
                ret.push_back(tok);
                tok.clear();
            }
        }
    }
    if(tok.size())
    {
        ret.push_back(tok);
        tok.clear();
    }
    return ret;
}

inline std::vector<Cell> GetTests(const std::string& text)
{
    auto cell = Parse(text);
    std::vector<Cell> ret;
    if(cell.type == Cell::Type::ROOT)
    {
        if(cell.list.size() == 0)
        {
            return ret;
        }
        for(const auto& c : cell.list)
        {
            const auto& l = c.list;
            if(l.size() > 1 && l[0].details.text == "test")
            {
                ret.push_back(c);
            }
        }
    }
}

struct GraphState
{
    std::map<std::string, std::vector<double>> inputs;
    std::map<std::string, std::vector<double>> observers;
};

void GetNode(const Cell& cell, std::vector<double>& vals)
{
    if(cell.type == Cell::Type::NUMBER)
    {
        vals.push_back(std::stod(cell.details.text));
        return;
    }
    else if(cell.type == Cell::Type::LIST)
    {
        for(auto c : cell.list)
        {
            GetNode(c, vals);
        }
        return;
    }
    assert(false);
}

inline std::tuple<bool, std::string, std::string> RunTest(IEngine& exysInstance, Cell test, GraphState& state)
{
    bool ret = true;
    std::string testname = test.list[1].details.text;
    std::string resultStr;
    assert(test.list.size() > 2);
    
    for(auto l = test.list.begin()+2; l != test.list.end(); ++l)
    {
        auto& firstElem = l->list.front();
        if(firstElem.details.text == "inject")
        {
            const auto& label = l->list[1].details.text;
            if(exysInstance.HasInputPoint(label))
            {
                auto& p = exysInstance.LookupInputPoint(label);
                auto val = l->list[2];
                if(val.type == Cell::Type::NUMBER)
                {
                    p = std::stod(val.details.text);
                }
                else if(val.type == Cell::Type::LIST)
                {
                    std::vector<double> nodeVals;
                    GetNode(val, nodeVals);
                    int i = 0;
                    for(auto v : nodeVals)
                    {
                        p[i++] = v;
                    }
                }
            }
        }
        else if(firstElem.details.text == "stabilize")
        {
            exysInstance.Stabilize();
            for(const auto& input : exysInstance.DumpInputs())
            {
                state.inputs[input.first].push_back(input.second);
            }
            for(const auto& observer : exysInstance.DumpObservers())
            {
                state.observers[observer.first].push_back(observer.second);
            }
        }
        else if(firstElem.details.text == "expect")
        {
            const auto& label = l->list[1].details.text;
            if(exysInstance.IsDirty())
            {
                resultStr += "Value checked before stabilization - " + label + "==" + l->list[2].details.text;
            }

            if(exysInstance.HasObserverPoint(label))
            {
                auto& p = exysInstance.LookupObserverPoint(label);
                auto val = l->list[2];
                if(val.type == Cell::Type::NUMBER)
                {
                    if(p != std::stod(val.details.text))
                    {
                        ret &= false;
                        resultStr += "Value does not meet expectation - " + label + "!=" 
                            + val.details.text + " actual " + std::to_string(p.mVal);
                    }
                }
                else if(val.type == Cell::Type::LIST)
                {
                    std::vector<double> nodeVals;
                    GetNode(val, nodeVals);
                    int i = 0;
                    for(auto v : nodeVals)
                    {
                        if(v != p[i].mVal)
                        {
                            ret &= false;
                            resultStr += "Value does not meet expectation - " + label + "!=" 
                                + std::to_string(v) + " actual " + std::to_string(p[i].mVal) + "\n";
                        }
                        ++i;
                    }
                }
            }
            else
            {
                ret &= false;
                resultStr += "Does not have observer - " + label;
            }
        }
    }
    return std::make_tuple(ret, testname, resultStr);
}

inline std::tuple<bool, std::string, GraphState> Execute(IEngine& exysInstance, const std::string& text)
{
    bool ret = true;
    std::string resultStr;
    GraphState state;

    const auto tests = GetTests(text);
    for(const auto test : tests)
    {
        bool success = false;
        std::string testname;
        std::string details;
        std::tie(success, testname, details) = RunTest(exysInstance, test, state);
        if(!success)
        {
            ret = false;
            resultStr += "[" + testname + "] " + details + "\n";
        }
    }
    if(ret)
    {
        resultStr = "All expectations met :)";
    }
    return std::make_tuple(ret, resultStr, state);
}

}
