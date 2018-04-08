#pragma once

#include <stdint.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <memory>
#include <deque>
#include <limits>
#include <set>
#include <ostream>
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
        KIND_BIND         = 1<<2,
        KIND_VAR          = 1<<3,
        KIND_STR          = 1<<4,
        KIND_LIST         = 1<<5,
        KIND_PROC         = 1<<6,
        KIND_PROC_FACTORY = 1<<7,
        KIND_GRAPH        = 1<<8
    };

    typedef std::shared_ptr<Node> Ptr;

    Node(Kind k) : mKind(k) {}
    virtual ~Node() {}

    const std::string& Label() 
    {
        return mToken;
    }

    Kind mKind=KIND_UNKNOWN;

    std::string mToken = "";
    std::vector<Ptr> mParents;
    std::vector<std::string> mInputLabels;
    std::vector<std::string> mObserverLabels;
    uint16_t mLength=1;
    bool mIsObserver = false;
    bool mIsInput = false;
    bool mForceKeep = false;
    
    // For use in later stages
    uint64_t mHeight = 0;
    int64_t mInputOffset = -1;
    int64_t mObserverOffset = -1;
    double mInitValue = 0.0;

    bool operator<(const Node& rhs) const
    {
        return (mHeight > rhs.mHeight);
    }
};

inline Node::Kind operator|(Node::Kind a, Node::Kind b) {return static_cast<Node::Kind>(static_cast<int>(a) | static_cast<int>(b));}

inline std::ostream& operator<<(std::ostream& os, Node::Kind kind)
{
    switch(kind)
    {
        case Node::Kind::KIND_UNKNOWN:      os << "unknown";           break;
        case Node::Kind::KIND_CONST:        os << "const";             break;
        case Node::Kind::KIND_VAR:          os << "variable";          break;
        case Node::Kind::KIND_BIND:         os << "binding";           break;
        case Node::Kind::KIND_LIST:         os << "list";              break;
        case Node::Kind::KIND_PROC:         os << "procedure";         break;
        case Node::Kind::KIND_PROC_FACTORY: os << "procedure Factory"; break;
        case Node::Kind::KIND_GRAPH:        os << "graph";             break;
    }
    return os;
}

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

    void Construct(const Cell& cell);
    void SetSupportedProcedures(const std::vector<Procedure>& procs);

    std::string GetDOTGraph() const;
    std::vector<Node::Ptr> GetLayout() const;
    std::vector<Node::Ptr> GetSimApplyLayout() const;

    std::string GetSimApplyTarget() const;

    std::vector<std::unique_ptr<Graph>> SplitOutBy(Node::Kind kind, const std::string& token);

private:
    Graph(std::vector<Node::Ptr> nodes);

    void DefineNode(const std::string& token, const Cell& cell);
    void DefineNode(const std::string& token, Node::Ptr node);

    Node::Ptr Build(const Cell& cell);
    ProcNodeFactoryFunc DefaultFactory(const Procedure& procedure);
    void AddProcFactory(const std::string id, ProcNodeFactoryFunc factory);
    Node::Ptr LookupSymbol(const Cell& cell);
    ProcNodeFactoryFunc LookupProcedure(const Cell& cell);
    void SetSymbol(const Cell& cell, Node::Ptr node);
    Node::Ptr BuildForProcedure(const Cell& token);
    void BuildInputList(Node::Ptr child, const std::string& token, std::deque<int> dims);
    void LabelListRoot(Node::Ptr node, std::string label, uint16_t length, bool inputLabel);
    void CollectInputs(Node::Ptr node, std::vector<Node::Ptr>& inputs) const;
    void CollectObservers(Node::Ptr node, std::vector<std::vector<Node::Ptr>>& observers) const;
    void CollectForceKeep(Node::Ptr node, std::vector<Node::Ptr>& nodes) const;
    
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
    Node::Ptr Nth(Node::Ptr node);
    Node::Ptr Format(Node::Ptr node);
    Node::Ptr Require(Node::Ptr node);
    Node::Ptr PrintLib(Node::Ptr node);

    template<typename T=Node, typename... Args>
    std::shared_ptr<T> BuildNode(Args... as);

    std::vector<Node::Ptr> mAllNodes;
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::vector<Cell> mProvidedNodes;

    Graph* mParent;
    Cell mCurrentCell;
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

