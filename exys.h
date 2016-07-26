#pragma once

#include <exception>
#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <map>

#include "node.h"
#include "parser.h"

namespace Exys
{

class Graph : public Node
{
public:
    Graph(Graph* parent=nullptr);

    void Stabilize(uint64_t stabilisationId) override
    {
    }
    
    Node::Ptr Build(const Cell& proc);
    void CompleteBuild();
    void DefineNode(const std::string& token, const Cell& exp);
    void DefineNode(const std::string& token, Node::Ptr node);
    
    Node::Ptr LookupInputNode(const std::string& label);
    Node::Ptr LookupObserverNode(const std::string& label);


    std::string GetDOTGraph();

    static std::unique_ptr<Graph> BuildGraph(const std::string& text);

private:
    Node::Ptr LookupSymbol(const Cell& cell);
    void SetSymbol(const Cell& cell, Node::Ptr node);
    SubGraphFactory LookupProcedure(const Cell& token);
    Node::Type InputType2Enum(const std::string& token);

    template<typename T, typename... Args>
    std::shared_ptr<T> BuildNode(Args... as);

    template<typename T>
    Node::Ptr BuildForProc(const ListNode& args);
    Node::Ptr ForEach(const ListNode& args);

    void RecursiveHeightSet(Node::Ptr node, uint64_t& height);
    
    std::vector<Node::Ptr> mAllNodes;
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::unordered_map<std::string, Node::Ptr> mObservers;
    std::unordered_map<std::string, Node::Ptr> mInputs;

    std::map<uint64_t, Node::Ptr> recomputeHeap; // height -> Nodes
    uint64_t stabilisationId=1;

    Graph* mParent;
};

class GraphBuildException : public std::exception
{
public:
    GraphBuildException(const std::string& error, Cell cell)
    : mError(error)
    , mCell(cell)
    {
    }

    virtual const char* what() const noexcept(true)
    {
        return mError.c_str();
    }

    std::string GetErrorMessage(const std::string& text) const
    {
        auto start = text.rfind("\n", mCell.offset);
        auto end = text.find("\n", mCell.offset);
        std::string errmsg(mError);
        errmsg += std::string(text, start, end-start);
        errmsg += "\n";
        for(int i = 1; i < mCell.offset-start; i++)
        {
            errmsg += " ";
        }
        errmsg += "^\n";
        return errmsg;
    }

    std::string mError;
    Cell mCell;
};


};
