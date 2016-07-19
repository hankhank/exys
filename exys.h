
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
    static Cell Number(const std::string& tok) { return {NUMBER, tok, {}};}
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
    virtual void Stabilise(uint64_t stabilisationId) 
    {
        mRecomputeId = stabilisationId;
        mChangeId = stabilisationId;
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

class InputNode : public Node
{
    InputNode() 
    : Node()
    , mKind(INPUT) {}
};

class TenaryNode : public Node
{
public:
    TenaryNode() 
    : Node()
    , mKind(TERNARY) {}

    void SetLegs(Node::Ptr condNode, Node::Ptr posNode, Node::ptr negNode)
    {
        mCondNode = condNode;
        mPosNode  = posNode;
        mNegNode  = negNode;
        mType = mNegNode->mType;
        mParents.push_back(condNode);
        mParents.push_back(posNode);
        mParents.push_back(negNode);
    }
    
    bool Bool()
    {
        switch(mCondNode->mType)
        {
            default: assert(false);
            case BOOL:   return mCondNode->mValue.b;
            case INT:    return mCondNode->mValue.i;
            case UINT:   return mCondNode->mValue.u;
            case DOUBLE: return mCondNode->mValue.d;
        }
    }

    virtual void Stabilise(uint64_t stabilisationId)
    {
        Node::Ptr leg = nullptr;
        if(Bool())
        {
            leg = mPosNode;
        }
        else
        {
            leg = mNegNode;
        }

        if(leg->mChangeId > mChangeId || leg != mCurLeg)
        {
            mValue = leg->mValue;
            mChangeId = stabilisationId;
            mCurLeg = leg;
        }
        mRecomputeId = stabilisationId;
    }
    
    Node::Ptr mCondNode;
    Node::Ptr mPosNode;
    Node::Ptr mNegNode;
    Node::Ptr mCurLeg;
};

class Graph
{
public:
    Graph() {}
    
private:
    Node::Ptr LookupNode(const std::string token);

    std::unorderd_map<const std::string, Node::Ptr> observers;
    std::map<uint64_t, Node::Ptr> recomputeHeap; // height -> Nodes
    std::unorderd_map<const std::string, Node::Ptr> mVarNodes;
    std::unorderd_map<const std::string, Node::Ptr> mInputsNodes;

    uint64_t mNextInputId=0;

    uint64_t stabilisationId=0;
    Graph* parent=nullptr;

    void Build(Cell proc, Node::Ptr childNode=nullptr);
};

};
