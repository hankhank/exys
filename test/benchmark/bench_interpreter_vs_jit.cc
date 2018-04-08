
#include "benchmark/benchmark.h"

#include "exys.h"
#include "interpreter.h"
#include "jitwrap.h"

enum
{
    RANDOM_SHUFFLE = 0,
    BEST_CASE,
    WORST_CASE,
};

template <typename T> 
void BM_ExecuteGraph_DeepSum(benchmark::State& state)
{
    std::string varins;
    std::string justnames;
    for(int i = 0; i < state.range(0); i++)
    {
        varins += "(input in" + std::to_string(i) + ") ";
        justnames += " in"+std::to_string(i);
    }
    std::string graph = "(begin " + varins + " (observe \"out\" (fold + 0 (list " + justnames + "))))";
    auto engine = T::Build(graph);
    
    // get inputs
    std::vector<Exys::Point*> inputs;
    
    switch (state.range(1))
    {
        default:
        case RANDOM_SHUFFLE:
        {
            for(auto& inputLabel : engine->GetInputPointLabels())
            {
                inputs.push_back(&engine->LookupInputPoint(inputLabel));
            }

            // Shuffle the order in which we update
            std::srand(state.range(1));
            std::random_shuffle(inputs.begin(), inputs.end());
        }
        break;
        case BEST_CASE:
        {
            inputs.push_back(&engine->LookupInputPoint("in"+std::to_string(state.range(0)-1)));
        }
        break;
        case WORST_CASE:
        {
            inputs.push_back(&engine->LookupInputPoint("in0"));
        }
        break;
    }

    // Try and warm cache
    for(auto* p : inputs)
    {
        *p = 1.0;
        engine->Stabilize();
    }

    size_t cur = 0, max = inputs.size();
    auto& output = engine->LookupObserverPoint("out");
    
    double adder = 1.0;
    while (state.KeepRunning()) 
    {
        auto& point = *inputs[cur];
        point = ++adder;
        engine->Stabilize();
        cur = (++cur) % max;
        //assert(output == (state.range(0)+adder));
    }
}

template <typename T> 
void BM_ExecuteGraph_FatSum(benchmark::State& state)
{
    std::string varins;
    std::string justnames;
    for(int i = 0; i < state.range(0); i++)
    {
        varins += "(input in" + std::to_string(i) + ") ";
        justnames += " in"+std::to_string(i);

    }
    std::string graph = "(begin " + varins + " (observe \"out\" (+ " + justnames + ")))";
    auto engine = T::Build(graph);

    // get inputs
    std::vector<Exys::Point*> inputs;
    
    switch (state.range(1))
    {
        default:
        case RANDOM_SHUFFLE:
        {
            for(auto& inputLabel : engine->GetInputPointLabels())
            {
                inputs.push_back(&engine->LookupInputPoint(inputLabel));
            }

            // Shuffle the order in which we update
            std::srand(state.range(1));
            std::random_shuffle(inputs.begin(), inputs.end());
        }
        break;
        case BEST_CASE:
        {
            inputs.push_back(&engine->LookupInputPoint("in"+std::to_string(state.range(0)-1)));
        }
        break;
        case WORST_CASE:
        {
            inputs.push_back(&engine->LookupInputPoint("in0"));
        }
        break;
    }

    // Try and warm cache
    for(auto* p : inputs)
    {
        *p = 1.0;
        engine->Stabilize();
    }

    size_t cur = 0, max = inputs.size();
    auto& output = engine->LookupObserverPoint("out");

    double adder = 1.0;
    while (state.KeepRunning()) 
    {
        auto& point = *inputs[cur];
        point = ++adder;
        engine->Stabilize();
        cur = (++cur) % max;
        //assert(output == (std::accumulate(&inputs[0], &inputs[max-1], 0)));
    }
}

BENCHMARK_TEMPLATE(BM_ExecuteGraph_FatSum, Exys::JitWrap)->Ranges({{8, 1024}, {0,2}});
BENCHMARK_TEMPLATE(BM_ExecuteGraph_FatSum, Exys::Interpreter)->Ranges({{8, 1024}, {0,2}});

BENCHMARK_TEMPLATE(BM_ExecuteGraph_DeepSum, Exys::JitWrap)->Ranges({{8, 1024}, {0,2}});
BENCHMARK_TEMPLATE(BM_ExecuteGraph_DeepSum, Exys::Interpreter)->Ranges({{8, 1024}, {0,2}});

BENCHMARK_MAIN()
