
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "exys.h"
#include "executioner.h"
#include "interpreter.h"
#include "jitwrap.h"

int main(int argc, char* argv[])
{
    enum { INTERPRETER, JITTER, GPU } mode = INTERPRETER;
    
    int opt;

    while ((opt = getopt(argc, argv, "ijg")) != -1) 
    {
        switch (opt) 
        {
            case 'i': mode = INTERPRETER; break;
            case 'j': mode = JITTER; break;
            case 'g': mode = GPU; break;
            default:
                fprintf(stderr, "Usage: %s [-ijg] file\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(optind > argc)
    {
        fprintf(stderr, "Usage: %s [-ijg] file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int ret = 0;
    for(int i = optind; i < argc; ++i)
    {
        std::cout << "Testing " << argv[i] << " - ";
        std::ifstream t(argv[i]);
        std::stringstream buffer;
        buffer << t.rdbuf();
        
        try
        {
            std::unique_ptr<Exys::IEngine> engine;
            if(mode == INTERPRETER)
            {
                engine = Exys::Interpreter::Build(buffer.str());
            }
#ifdef JIT
            else if(mode == JITTER)
            {
                engine = Exys::JitWrap::Build(buffer.str());
            }
#endif
            else
            {
                assert(false);
            }
            auto results = Exys::Execute(*engine, buffer.str());
            std::cout << std::get<1>(results) << "\n";
            if(!std::get<0>(results)) ret = -1;
        }
        catch (const Exys::ParseException& e)
        {
            std::cout << e.GetErrorMessage(buffer.str());
        }
        catch (const Exys::GraphBuildException& e)
        {
            std::cout << e.GetErrorMessage(buffer.str());
        }

    }
    return ret;
}
