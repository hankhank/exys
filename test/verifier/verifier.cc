
#include <iostream>
#include <fstream>
#include <sstream>

#include "exys.h"
#include "executioner.h"
#include "interpreter.h"
#include "jitter.h"

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
        auto graph = Exys::Jitter::Build(buffer.str());
        auto results = Exys::Execute(*graph, buffer.str());
        std::cout << std::get<1>(results) << "\n";
        return std::get<0>(results) ? 0 : -1;
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
