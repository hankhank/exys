#pragma once

#include <string>

inline std::string GetLine(const std::string& text, int line)
{
    int lineNumber = 0;
    std::string::size_type pos = 0, prev = 0;
    while ((pos = text.find('\n', prev)) != std::string::npos)
    {
        if(++lineNumber > line) break;
        prev = pos + 1;
    }
    return text.substr(prev, pos == std::string::npos ? std::string::npos : pos - prev);
}
