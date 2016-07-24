
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
    int64_t offset;
    std::vector<Cell> list;
    
    static Cell Symbol(const std::string& tok, int offset) 
    { return {SYMBOL, tok, offset, {}};}
    static Cell Number(const std::string& tok, int offset) 
    { return {NUMBER, tok, offset, {}};}
    static Cell List(int offset) 
    { return {LIST, "", offset, {}};}
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
    virtual void Stabilize(uint64_t stabilisationId) 
    {
        mRecomputeId = stabilisationId;
        mChangeId = stabilisationId;
    }

    virtual std::string Label()
    {
        return std::to_string((long int)this);
    }

    Kind mKind=KIND_UNKNOWN;
    Type mType=TYPE_UNKNOWN;

    uint64_t mRecomputeId=0;
    uint64_t mChangeId=0;
    uint64_t mHeight=0;

    std::vector<Ptr> mParents;
    
    Var mValue;
};

class Graph
{
public:
    Graph();
    
    Node::Ptr Build(const Cell& proc);
    
    typedef std::function<Node::Ptr (const std::vector<Cell>&)>
        GraphFactory;

    Node::Ptr LookupInputNode(const std::string& label);
    Node::Ptr LookupObserverNode(const std::string& label);

    void Stabilize();

    std::string GetDOTGraph();

    static std::unique_ptr<Graph> BuildGraph(const std::string& text);

//private:
    void CompleteBuild();
    Node::Ptr LookupSymbol(const Cell& cell);
    void SetSymbol(const Cell& cell, Node::Ptr node);
    GraphFactory LookupProcedure(const Cell& token);

    Node::Type InputType2Enum(const std::string& token);

    template<typename T>
    Node::Ptr BuildForProc(const std::vector<Cell>& args);

    template<typename T>
    std::shared_ptr<T> BuildNode();
    
    std::vector<Node::Ptr> mAllNodes;
    std::map<uint64_t, Node::Ptr> recomputeHeap; // height -> Nodes
    std::unordered_map<std::string, Node::Ptr> mVarNodes;
    std::unordered_map<std::string, Node::Ptr> mObservers;
    std::unordered_map<std::string, Node::Ptr> mInputs;
    std::unordered_map<std::string, GraphFactory> mProcs;
    std::map<uint64_t, Node::Ptr> mNecessaryNodes;

    uint64_t stabilisationId=1;
    Graph* mParent=nullptr;

    std::vector<std::shared_ptr<Graph>> mSubGraphs;

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

class ProcNode : public Node
{
public:
    ProcNode() 
    : Node(KIND_PROC) {}

    Graph::GraphFactory mFactory;
};

class ConstNode : public Node
{
public:
    ConstNode() 
    : Node(KIND_CONST) {}

    virtual ~ConstNode(){}

    void ReadConstant(const std::string& token)
    {
        try
        {
            mValue.i = std::stoi(token);
            mType = TYPE_INT;
            return;
        }
        catch (std::invalid_argument)
        {
        }
        try
        {
            mValue.d = std::stof(token);
            mType = TYPE_DOUBLE;
            return;
        }
        catch (std::invalid_argument)
        {
        }
    }

    virtual std::string Label() override
    {
        switch(mType)
        {
            default:
            case TYPE_BOOL:   return std::to_string(mValue.b);
            case TYPE_INT:    return std::to_string(mValue.i);
            case TYPE_UINT:   return std::to_string(mValue.u);
            case TYPE_DOUBLE: return std::to_string(mValue.d);
        }
    }
};

class InputNode : public Node
{
public:
    InputNode() 
    : Node(KIND_INPUT) {}

    virtual ~InputNode(){}

    virtual std::string Label() override
    {
        return mToken;
    }

    std::string mToken;
};

#define MATH_NODE(__NAME, __OP) \
class __NAME : public Node \
{\
public: \
    __NAME() \
    : Node(KIND_VAR) \
    {} \
    virtual ~__NAME(){} \
    \
    virtual void Stabilize(uint64_t stabilisationId)\
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
\
    virtual std::string Label() override \
    { \
        return std::string(# __OP); \
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

    virtual std::string Label()
    {
        return "?";
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

    virtual void Stabilize(uint64_t stabilisationId)
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


};
