#include <iostream>

#include "exys.h"

int main()
{
    const auto& d = std::string("(define res 0)\n(set! res (+ res 1))");
    
    try
    {
        auto graph = Exys::Graph::BuildGraph(d);
        std::cout << "built";
    }
    catch (const Exys::GraphBuildException& e)
    {
        std::cout << e.GetErrorMessage(d);
    }

    return 0;
}
