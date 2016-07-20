
#include <memory>
#include <cstdint>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <functional>
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

class Node
{
public:
    enum Kind
    {
        KIND_UNKNOWN=0,
        KIND_CONST,
        KIND_VAR,
        KIND_TERNARY,
        KIND_INPUT,
        KIND_OBSERVER,
        KIND_PROC,
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
    typedef union
    {
        bool         b;
        int          i;
        unsigned int u;
        double       d;
    } Var; 

    Node(Kind k)
    : mKind(k)
    {}

    virtual ~Node() {}
    virtual void Stabilise(uint64_t stabilisationId) 
    {
        mRecomputeId = stabilisationId;
        mChangeId = stabilisationId;
    }

    Kind mKind=KIND_UNKNOWN;
    Type mType=TYPE_UNKNOWN;
    std::string mToken="";
    uint64_t mRecomputeId=0;
    uint64_t mChangeId=0;
    uint64_t mHeight=0;

    std::vector<Ptr> mParents;
    
    Var mValue;
};

class ConstNode : public Node
{
public:
    ConstNode() 
    : Node(KIND_CONST) {}
};

class InputNode : public Node
{
public:
    InputNode() 
    : Node(KIND_INPUT) {}
};

#define MATH_NODE(__NAME, __OP) \
class __NAME : public Node \
{\
public: \
    __NAME() \
    : Node(KIND_VAR) \
    {} \
    \
    virtual void Stabilise(uint64_t stabilisationId)\
    {\
        Var newVal; \
        bool update = false; \
        for(auto leg : mParents) \
        { \
            switch(mType) \
            { \
                case TYPE_BOOL:   newVal.b __OP leg->mValue.b; break; \
                case TYPE_INT:    newVal.i __OP leg->mValue.i; break; \
                case TYPE_UINT:   newVal.u __OP leg->mValue.u; break; \
                case TYPE_DOUBLE: newVal.d __OP leg->mValue.d; break; \
            } \
            update |= leg->mChangeId > mChangeId; \
        } \
        if(update) \
        { \
            mValue = newVal; \
            mChangeId = stabilisationId; \
        } \
        mRecomputeId = stabilisationId; \
    } \
};

MATH_NODE(SumNode, +)
MATH_NODE(SubNode, -)
MATH_NODE(DivNode, /)
MATH_NODE(MulNode, *)

class TenaryNode : public Node
{
public:
    TenaryNode() 
    : Node(KIND_TERNARY)
    {}

    void SetLegs(Node::Ptr condNode, Node::Ptr posNode, Node::Ptr negNode)
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
            default: // add assert
            case TYPE_BOOL:   return mCondNode->mValue.b;
            case TYPE_INT:    return mCondNode->mValue.i;
            case TYPE_UINT:   return mCondNode->mValue.u;
            case TYPE_DOUBLE: return mCondNode->mValue.d;
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
    Graph();
    
    Node::Ptr Build(const Cell& proc);
    
    typedef std::function<Node::Ptr (const std::vector<Cell>&)>
        GraphFactory;

    static std::unique_ptr<Graph> BuildGraph(const std::string& text);

private:
    Node::Ptr LookupSymbol(const std::string& token);
    GraphFactory LookupProcedure(const std::string& token);

    Node::Type InputType2Enum(const std::string& token);

    template<typename T>
    Node::Ptr BuildNode(const std::vector<Cell>& args);

    std::unordered_map<std::string, Node::Ptr> observers;
    std::map<uint64_t, Node::Ptr> recomputeHeap; // height -> Nodes
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::unordered_map<std::string, Node::Ptr> mInputsNodes;
    std::unordered_map<std::string, GraphFactory> mProcs;

    uint64_t mNextInputId=0;

    uint64_t stabilisationId=0;
    Graph* parent=nullptr;
};


};
