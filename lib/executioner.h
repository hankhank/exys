#pragma once

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

namespace Exys
{

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
    return ret;
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
        if (!l->list.size())
        {
            ret &= false;
            resultStr += "Syntax error - expected start of list";
            break;
        }
        auto& firstElem = l->list.front();
        if(firstElem.details.text == "inject")
        {
            if(l->list.size() < 3)
            {
				ret &= false;
				resultStr += "Not enough arguments for inject\n";
				break;
			}
            if(l->list.size() > 3)
            {
				ret &= false;
				resultStr += "Too many arguments for inject\n";
				break;
			}

            const auto& label = l->list[1].details.text;
            if(!exysInstance.HasInputPoint(label))
            {
				ret &= false;
				resultStr += "Unrecognised input - " + label;
				break;
			}

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
        else if(firstElem.details.text == "sim-capture")
        {
            exysInstance.CaptureState();
        }
        else if(firstElem.details.text == "sim-reset")
        {
            exysInstance.ResetState();
        }
        else if(firstElem.details.text == "dump-inputs")
        {
            auto inputs = exysInstance.DumpInputs();
            std::sort(inputs.begin(), inputs.end());
            std::cout << "Input values:\n";
            for(auto& input : inputs)
            {
                std::cout << input.first << " = " << input.second << "\n";
            }
        }
        else if(firstElem.details.text == "dump-observers")
        {
            auto observers = exysInstance.DumpObservers();
            std::sort(observers.begin(), observers.end());
            std::cout << "Observer values:\n";
            for(auto& observer : observers)
            {
                std::cout << observer.first << " = " << observer.second << "\n";
            }
        }
        else if(firstElem.details.text == "sim-run")
        {
            if(l->list.size() != 2)
            {
				ret &= false;
				resultStr += "Not enough arguments for sim-run\n";
				break;
            }

			int id = std::stoi(l->list[1].details.text);
            if(id >= exysInstance.GetNumSimulationFunctions())
            {
				ret &= false;
				resultStr += "Simulation id does not exist\n";
				break;
            }

            int simRun = 0;
            while(simRun < 1000 && !exysInstance.RunSimulationId(id))
            {
                ++simRun;
            }
            if(simRun >= 1000)
            {
				ret &= false;
				resultStr += "Too many simulations\n";
				break;
            }
        }
        else if(firstElem.details.text == "expect")
        {
            if(l->list.size() < 3)
            {
				ret &= false;
				resultStr += "Not enough arguments for expect\n";
				break;
			}

            const auto& label = l->list[1].details.text;
            if(exysInstance.IsDirty())
            {
                resultStr += "Value checked before stabilization - " + label + "==" + l->list[2].details.text + "\n";
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
                            + val.details.text + " actual " + std::to_string(p.mVal) + "\n";
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
                resultStr += "Does not have observer - " + label + "\n";
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
            resultStr += "[" + testname + "]\n" + details + "\n";
        }
    }
    if(ret)
    {
        resultStr = "All expectations met :)";
    }
    return std::make_tuple(ret, resultStr, state);
}

}
