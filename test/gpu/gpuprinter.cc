
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "exys.h"
#include "gputer.h"

int main(int argc, char* argv[])
{
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
        
    try
    {
        auto gputer = Exys::Gputer::Build(buffer.str());
        std::cout << gputer->GetPTX();
    }
    catch (const Exys::ParseException& e)
    {
        std::cout << e.GetErrorMessage(buffer.str());
    }
    catch (const Exys::GraphBuildException& e)
    {
        std::cout << e.GetErrorMessage(buffer.str());
    }

    return 0;
}
