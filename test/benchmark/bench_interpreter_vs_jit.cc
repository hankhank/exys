
#include "benchmark/benchmark.h"

#include "exys.h"
#include "interpreter.h"
#include "jitter.h"

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
    for(int i = 0; i < state.range(0); i++)
    {
        varins += "in" + std::to_string(i) + " ";
    }
    std::string graph = "(begin (input double " + varins + ") (observe out (fold + 0 (list " + varins + "))))";
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
        engine->PointChanged(*p);
        engine->Stabilize();
    }

    size_t cur = 0, max = inputs.size();
    auto& output = engine->LookupObserverPoint("out");

    while (state.KeepRunning()) 
    {
        auto& point = *inputs[cur];
        point = 1.0;
        engine->PointChanged(point);
        engine->Stabilize();
        cur = (++cur) % max;
        assert(output == state.range(0));
    }
}

template <typename T> 
void BM_ExecuteGraph_FatSum(benchmark::State& state)
{
    std::string varins;
    for(int i = 0; i < state.range(0); i++)
    {
        varins += "in" + std::to_string(i) + " ";
    }
    std::string graph = "(begin (input double " + varins + ") (observe out (+ " + varins + ")))";
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
        engine->PointChanged(*p);
        engine->Stabilize();
    }

    size_t cur = 0, max = inputs.size();
    auto& output = engine->LookupObserverPoint("out");

    while (state.KeepRunning()) 
    {
        auto& point = *inputs[cur];
        point = 1.0;
        engine->PointChanged(point);
        engine->Stabilize();
        cur = (++cur) % max;
        assert(output == state.range(0));
    }
}

BENCHMARK_TEMPLATE(BM_ExecuteGraph_FatSum, Exys::Jitter)->Ranges({{8, 1024}, {0,2}});
BENCHMARK_TEMPLATE(BM_ExecuteGraph_FatSum, Exys::Interpreter)->Ranges({{8, 1024}, {0,2}});

BENCHMARK_TEMPLATE(BM_ExecuteGraph_DeepSum, Exys::Jitter)->Ranges({{8, 1024}, {0,2}});
BENCHMARK_TEMPLATE(BM_ExecuteGraph_DeepSum, Exys::Interpreter)->Ranges({{8, 1024}, {0,2}});

BENCHMARK_MAIN()
