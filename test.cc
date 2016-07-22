#include <iostream>
#include <fstream>
#include <sstream>

#include "exys.h"

int main(int argc, char* argv[])
{
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    
    try
    {
        auto graph = Exys::Graph::BuildGraph(buffer.str());
        auto input = graph->LookupInputNode("res");
        auto output = graph->LookupObserverNode("res");
        std::cout << graph->GetDOTGraph();
    }
    catch (const Exys::GraphBuildException& e)
    {
        std::cout << e.GetErrorMessage(buffer.str());
    }

    return 0;
}
