
#include <gtest/gtest.h>

#include "parser.h"

std::ostream& operator<<(std::ostream& s, Exys::TokenDetails const & d)
{
    s << "Token='" << d.text << "' (" 
        << d.firstLineNumber << ", " << d.firstColumn << ", "
        << d.endLineNumber << ", " << d.endColumn << ")";
    return s;
}

namespace Exys { namespace test {

bool operator==(const TokenDetails a, const TokenDetails b)
{
    return 
    (
        a.text == b.text &&
        a.firstLineNumber == b.firstLineNumber &&
        a.firstColumn == b.firstColumn &&
        a.endLineNumber == b.endLineNumber &&
        a.endColumn == b.endColumn
    );
}

std::list<std::string> GetOpenCloseParens()
{
    std::list<std::string> ret;
    for(int i = 0; i < 101; i++)
    {
        ret.push_back(i % 2 ? "(" : ")");
    }
    return ret;
}

std::list<std::string> GetGeneralExample()
{
    return {"(", "begin", "(", "sum", "1", "2", "3", ")", "(", "reallllyyyylllonnngggnnnammeeE", ")", ")"};
}

std::pair<std::string, std::list<TokenDetails>> GenerateCompactTokenString(std::list<std::string> toks)
{
    std::string ret;
    std::list<TokenDetails> details;
    int column=0;
    for(auto& tok : toks)
    {
        TokenDetails detail;
        detail.text = tok;
        detail.firstLineNumber = 0;
        detail.firstColumn = column;
        detail.endLineNumber = 0;
        detail.endColumn = column + tok.size() -1;
        details.emplace_back(detail);

        ret += tok;
        column += tok.size();
        if(tok != "(" && tok != ")")
        {
            ret += " ";
            column += 1;
        }
    }
    return std::make_pair(ret, details);
}

std::pair<std::string, std::list<TokenDetails>> GenerateSpaceExpandedTokenString(std::list<std::string> toks)
{
    std::string ret;
    std::list<TokenDetails> details;
    int column=0;
    int newline=0;
    for(auto& tok : toks)
    {
        TokenDetails detail;
        detail.text = tok;
        detail.firstLineNumber = newline;
        detail.firstColumn = column;
        detail.endLineNumber = newline;
        detail.endColumn = column + tok.size() -1;
        details.emplace_back(detail);

        ret += tok;
        column += tok.size();
        for(int i = 0; i < column + 1; i++)
        {
            if (i % 3 == 0)
            {
                ret += " ";
                column += 1;
            }
            else
            {
                ret += "\n";
                column = 0;
                newline += 1;
            }
        }
    }
    return std::make_pair(ret, details);
}

std::pair<std::string, std::list<TokenDetails>> GenerateNewlineExpandedTokenString(std::list<std::string> toks)
{
    std::string ret;
    std::list<TokenDetails> details;
    int newline=0;
    for(auto& tok : toks)
    {
        TokenDetails detail;
        detail.text = tok;
        detail.firstLineNumber = newline;
        detail.firstColumn = 0;
        detail.endLineNumber = newline;
        detail.endColumn = tok.size() -1;
        details.emplace_back(detail);

        ret += tok;
        ret += "\n";
        newline += 1;
    }
    return std::make_pair(ret, details);
}

std::pair<std::string, std::list<TokenDetails>> GenerateCommentedTokenString(std::list<std::string> toks)
{
    std::string ret;
    std::list<TokenDetails> details;
    int column=0;
    int newline=0;
    for(auto& tok : toks)
    {
        TokenDetails detail;
        detail.text = tok;
        detail.firstLineNumber = newline;
        detail.firstColumn = column;
        detail.endLineNumber = newline;
        detail.endColumn = column + tok.size() -1;
        details.emplace_back(detail);

        ret += tok;
        column += tok.size();
        for(int i = 0; i < column + 1; i++)
        {
            if (i % 3 == 0)
            {
                ret += " ";
                column += 1;
            }
            else
            {
                ret += "; (looks (like something)) but is nothing\n";
                column = 0;
                newline += 1;
            }
        }
    }
    return std::make_pair(ret, details);
}

void CompareTokenDetails(std::list<TokenDetails>& expected, std::list<TokenDetails>& result)
{
    int i = 0;
    ASSERT_EQ(expected.size(), result.size());
    auto ei = expected.begin();
    auto ri = result.begin();
    while(ei != expected.end())
    {
        SCOPED_TRACE(::testing::Message() << "Comparing " << *ei << " " << *ri);
        ASSERT_TRUE(*ei == *ri);
        ei++;
        ri++;
    }
}

void RunTest(std::string input, std::list<TokenDetails> expected)
{
    SCOPED_TRACE(::testing::Message() << "Input " << input);
    auto details = Tokenize(input);
    CompareTokenDetails(expected, details);
}

TEST(Tokenize, empty)
{   
    auto details = Tokenize("");
    ASSERT_EQ(0, details.size());
}

TEST(Tokenize, JustParenese_Compact)
{   
    auto resultdata = GenerateCompactTokenString(GetOpenCloseParens());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, JustParenese_Spaced)
{   
    auto resultdata = GenerateSpaceExpandedTokenString(GetOpenCloseParens());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, JustParenese_Newline)
{   
    auto resultdata = GenerateNewlineExpandedTokenString(GetOpenCloseParens());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, JustParenese_Comments)
{   
    auto resultdata = GenerateCommentedTokenString(GetOpenCloseParens());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, GeneralExample_Compact)
{   
    auto resultdata = GenerateCompactTokenString(GetGeneralExample());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, GeneralExample_Spaced)
{   
    auto resultdata = GenerateSpaceExpandedTokenString(GetGeneralExample());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, GeneralExample_Newline)
{   
    auto resultdata = GenerateNewlineExpandedTokenString(GetGeneralExample());
    RunTest(resultdata.first, resultdata.second);
}

TEST(Tokenize, GeneralExample_Comments)
{   
    auto resultdata = GenerateCommentedTokenString(GetGeneralExample());
    RunTest(resultdata.first, resultdata.second);
}

}}
