
#include "benchmark/benchmark.h"

#include "exys.h"
#include "interpreter.h"
#include "jitter.h"

template <typename T> 
void BM_ExecuteGraph_SimpleSum(benchmark::State& state)
{
    std::string varins;
    for(int i = 0; i < state.range(1); i++)
    {
        varins += "in" + std::to_string(i) + " ";
    }
    std::string graph = "(begin (input double " + varins + ") (observe out (/ " + varins + ")))";
    auto engine = T::Build(graph);
    
    // get inputs
    std::vector<Exys::Point*> inputs;
    for(auto& inputLabel : engine->GetInputPointLabels())
    {
        inputs.push_back(&engine->LookupInputPoint(inputLabel));
    }

    // Shuffle the order in which we update
    std::srand(state.range(0));
    std::random_shuffle(inputs.begin(), inputs.end());

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
        assert(output == state.range(1));
    }
}

BENCHMARK_TEMPLATE(BM_ExecuteGraph_SimpleSum, Exys::Jitter)->Ranges({{8, 8<<10}, {8, 4096}});
BENCHMARK_TEMPLATE(BM_ExecuteGraph_SimpleSum, Exys::Interpreter)->Ranges({{8, 8<<10}, {8, 4096}});

BENCHMARK_MAIN()
