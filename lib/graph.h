#pragma once

#include <stdint.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <memory>
#include <limits>
#include <unordered_map>

#include "parser.h"

namespace Exys
{

class Node
{
public:
    enum Kind
    {
        KIND_UNKNOWN=0,
        KIND_CONST,
        KIND_LIST,
        KIND_INPUT,
        KIND_OBSERVER,
        KIND_PROC,
        KIND_PROC_FACTORY,
        KIND_GRAPH
    };

    enum Type
    {
        TYPE_UNKNOWN=0,
        TYPE_BOOL,
        TYPE_INT,
        TYPE_UINT,
        TYPE_DOUBLE
    };

    typedef std::shared_ptr<Node> Ptr;

    Node(Kind k) : mKind(k) {}
    virtual ~Node() {}

    const std::string& Label() {return mToken;}

    Kind mKind=KIND_UNKNOWN;
    Type mType=TYPE_UNKNOWN;

    std::vector<Ptr> mParents;
    std::string mToken;
    uint64_t mHeight;
};

typedef std::function<Node::Ptr (Node::Ptr)> ProcNodeFactoryFunc;

class ProcNodeFactory : public Node
{
public:
    ProcNodeFactory(ProcNodeFactoryFunc factory)
    : Node(KIND_PROC_FACTORY)
    , mFactory(factory)
    {
    }

    ProcNodeFactoryFunc mFactory;
};

class Graph : public Node
{
public:
    Graph(Graph* parent=nullptr);

    Node::Ptr Build(const Cell& cell);
    void DefineNode(const std::string& token, const Cell& cell);
    void DefineNode(const std::string& token, Node::Ptr node);

    const std::unordered_map<std::string, Node::Ptr>& GetObservers();

    void SetSupportedProcedures(const std::vector<std::string>& procs);
    
    std::string GetDOTGraph();

private:
    ProcNodeFactoryFunc DefaultFactory(const std::string& procname);
    void AddProcFactory(const std::string id, ProcNodeFactoryFunc factory);
    Node::Ptr LookupSymbol(const Cell& cell);
    ProcNodeFactoryFunc LookupProcedure(const Cell& cell);
    void SetSymbol(const Cell& cell, Node::Ptr node);
    Node::Ptr BuildForProcedure(const Cell& token);
    Node::Type InputType2Enum(const std::string& token);
    
    // Graph manipulation functions
    Node::Ptr Map(Node::Ptr node);
    Node::Ptr ForEach(Node::Ptr node);
    Node::Ptr List(Node::Ptr node);
    Node::Ptr Zip(Node::Ptr node);
    Node::Ptr Car(Node::Ptr node);
    Node::Ptr Cdr(Node::Ptr node);

    template<typename T=Node, typename... Args>
    std::shared_ptr<T> BuildNode(Args... as);

    std::vector<Node::Ptr> mAllNodes;
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::unordered_map<std::string, Node::Ptr> mObservers;

    Graph* mParent;
};

class GraphBuildException : public std::exception
{
public:
    GraphBuildException(const std::string& error, Cell cell);

    virtual const char* what() const noexcept(true);
    std::string GetErrorMessage(const std::string& text) const;

    std::string mError;
    Cell mCell;
};

}

