#pragma once

#include <iostream>
#include <fstream>
#include <sstream>

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

inline std::vector<std::string> GetPotentialComputeLines(const std::string& text)
{
    std::vector<std::string> ret;
    std::istringstream f(text);
    std::string s;    
    while (getline(f, s)) 
    {
        if(s.size() > 1 && s[0] == ';')
        {
            ret.push_back(s.substr(1));
        }
    }
    return ret;
}

struct GraphState
{
    std::map<std::string, std::vector<double>> inputs;
    std::map<std::string, std::vector<double>> observers;
};

inline std::tuple<bool, std::string, GraphState> Execute(Exys& exysInstance, const std::string& text)
{
    bool ret = true;
    std::string resultStr;
    GraphState state;

    const auto potCompLines = GetPotentialComputeLines(text);
    for(const auto pcl : potCompLines)
    {
        const auto sl = SplitLine(pcl);
        if(sl.size() > 2 && sl[0] == "inject")
        {
            const auto& label = sl[1];
            if(exysInstance.HasInputPoint(label))
            {
                auto& p = exysInstance.LookupInputPoint(label);
                p = std::stod(sl[2]);
                exysInstance.PointChanged(p);
            }
        }
        else if(sl.size() && sl[0] == "stabilize")
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
        else if(sl.size() > 2 && sl[0] == "expect")
        {
            const auto& label = sl[1];
            if(exysInstance.IsDirty())
            {
                resultStr += "Value checked before stabilization - " + label + "==" + sl[2];
            }

            if(exysInstance.HasObserverPoint(label))
            {
                auto& p = exysInstance.LookupObserverPoint(label);
                if(p != std::stod(sl[2]))
                {
                    ret &= false;
                    resultStr += "Value does not meet expectation - " + label + "!=" + sl[2] + " actual " + std::to_string(p.mSignal.d);
                }
            }
        }
    }
    if(ret)
    {
        resultStr = "All expectations met :)";
    }
    return std::make_tuple(ret, resultStr, state);
}
}
