#pragma once

#include <stdint.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <memory>
#include <deque>
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
        KIND_UNKNOWN      = 1<<0,
        KIND_CONST        = 1<<1,
        KIND_VAR          = 1<<2,
        KIND_LIST         = 1<<3,
        KIND_PROC         = 1<<4,
        KIND_PROC_FACTORY = 1<<5,
        KIND_GRAPH        = 1<<6
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

    std::string mToken = "";
    std::vector<Ptr> mParents;
    std::vector<std::string> mObserverLabels;
    bool mIsObserver = false;
    bool mIsInput = false;
    
    // Not populated in graph.cc but for use by later stages
    uint64_t mHeight = 0;

    bool operator<(const Node& rhs) const
    {
        return (mHeight > rhs.mHeight);
    }
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

typedef std::function<void (Node::Ptr)> ProcedureValidationFunction;
struct Procedure
{
    const char* id;
    ProcedureValidationFunction validate;
};

class Graph : public Node
{
public:
    Graph(Graph* parent=nullptr);

    Node::Ptr Construct(const Cell& cell);
    void DefineNode(const std::string& token, const Cell& cell);
    void DefineNode(const std::string& token, Node::Ptr node);

    const std::unordered_map<std::string, Node::Ptr>& GetObservers();
    const std::unordered_map<std::string, Node::Ptr>& GetInputs();

    void SetSupportedProcedures(const std::vector<Procedure>& procs);
    
    std::string GetDOTGraph();

    std::vector<Node::Ptr> GetStandardLayout();

private:
    Node::Ptr Build(const Cell& cell);
    ProcNodeFactoryFunc DefaultFactory(const Procedure& procedure);
    void AddProcFactory(const std::string id, ProcNodeFactoryFunc factory);
    Node::Ptr LookupSymbol(const Cell& cell);
    ProcNodeFactoryFunc LookupProcedure(const Cell& cell);
    void SetSymbol(const Cell& cell, Node::Ptr node);
    Node::Ptr BuildForProcedure(const Cell& token);
    Node::Type InputType2Enum(const std::string& token);
    void BuildInputList(Node::Ptr child, std::string token, std::deque<int> dims);
    void LabelObserver(Node::Ptr observer, std::string token);
    
    // Graph manipulation functions
    Node::Ptr Map(Node::Ptr node);
    Node::Ptr ForEach(Node::Ptr node);
    Node::Ptr List(Node::Ptr node);
    Node::Ptr Zip(Node::Ptr node);
    Node::Ptr Car(Node::Ptr node);
    Node::Ptr Cdr(Node::Ptr node);
    Node::Ptr Fold(Node::Ptr node);
    Node::Ptr Iota(Node::Ptr node);
    Node::Ptr Import(Node::Ptr node);
    Node::Ptr Apply(Node::Ptr node);
    Node::Ptr Append(Node::Ptr node);

    template<typename T=Node, typename... Args>
    std::shared_ptr<T> BuildNode(Args... as);

    std::vector<Node::Ptr> mAllNodes;
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::unordered_map<std::string, Node::Ptr> mObservers;
    std::unordered_map<std::string, Node::Ptr> mInputs;

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

