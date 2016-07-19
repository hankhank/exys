
#include <memory>
#include <cstdint>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <list>

namespace Exys
{

struct Cell
{
    enum Type 
    {
        LIST,
        SYMBOL,
        NUMBER
    };

    Type type;
    std::string token;
    std::vector<Cell> list;
    
    static Cell Symbol(const std::string& tok) { return {SYMBOL, tok, {}};}
    static Cell List() { return {LIST, "", {}};}
};

// Supported operations
// +
// -
// /
// *
// exp
// ln
// define
// list
// for-each
// output
// lambda
//

class Node
{
    enum Kind
    {
        UNKNOWN=0,
        CONST,
        VAR,
        TENARY,
        INPUT,
        OBSERVER,
        PROC,
        GRAPH
    };
    enum Type
    {
        UNKNOWN=0,
        BOOL,
        INT,
        UINT,
        DOUBLE
    };
    typedef std::shared_ptr<Node> Ptr;

public:
    virtual ~Node() {}
    virtual void Recompute() {};
    virtual void Stabilise()
    {
        for(auto parent : mParents)
        {
            if(parent->mChangeId > mChangeId)
            {
                Recompute();
            }
        }
    }

    Kind mKind=UNKNOWN;
    Type mType=UNKNOWN;
    std::string mToken="";
    uint64_t mRecomputeId=0;
    uint64_t mChangeId=0;
    uint64_t mHeight=0;

    std::vector<Ptr> mParents;
    std::vector<Ptr> mChildren;

    union mValue
    {
        bool         b;
        int          i;
        unsigned int u;
        double       d;
    }; 
};

class ConstNode : public Node
{
    ConstNumber() 
    : Node()
    , mKind(CONST) {}
};

class TenaryNode : public Node
{
public:
    TenaryNode() 
    : Node()
    , mKind(TERNARY) {}
};

class Graph
{
public:
    Graph() {}
    
    void RegisterNode(Node::Ptr node);
    Node::Ptr LookupNode(const std::string token);
    void SetMostRecentVarNode(const std::string varToken, Node::Ptr varNode);

private:
    std::set<Node::Ptr> observers;
    std::map<uint64_t, Node::Ptr> recomputeHeap; // height -> Nodes
    std::unorderd_map<const std::string, Node::Ptr> mVarNodes;

    uint64_t mNextInputId=0;

    uint64_t stabilisationId=0;
    Graph* parent=nullptr;

    void Build(Cell proc);
};

};
