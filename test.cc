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
        auto graph = Exys::Exys::Build(buffer.str());
        auto& input = graph->LookupInputPoint("in");
        auto& output = graph->LookupObserverPoint("out");

        input = 5.0;
        graph->FlagChanged(input);
        graph->Stabilize();

        std::cout << "digraph Exys " << graph->GetDOTGraph();
        std::cout << output.mSignal.d << "\n";
    }
    catch (const Exys::GraphBuildException& e)
    {
        std::cout << e.GetErrorMessage(buffer.str());
    }

    return 0;
}
