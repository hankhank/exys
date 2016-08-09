
#include <iostream>
#include <fstream>
#include <sstream>

#include "exys.h"

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        std::cout << "usage: verifier input_file\n";
        return -1;
    }
    
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    
    try
    {
        auto graph = Exys::Exys::Build(buffer.str());
        //auto& input = graph->LookupInputPoint("in");
        //auto& output = graph->LookupObserverPoint("out");
        return 0;
    }
    catch (const Exys::ParseException& e)
    {
        std::cout << e.GetErrorMessage(buffer.str());
    }
    catch (const Exys::GraphBuildException& e)
    {
        std::cout << e.GetErrorMessage(buffer.str());
    }

    return -1;
}
