#ifndef _WIN32
#pragma once

#include <string>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <set>

#include "exys.h"
#include "jitter.h"

namespace Exys
{

class SimX86 : public Jitter
{
    void Stabilize(void* inputs, void* outputs);
    void SimulateStabilize(void* inputs, void* outputs);
};

class SimGpu : public Jitter
{
    std::string GetKernelAsm() const;
    std::string GetInterpreterAsm() const;
    std::string GetSimAsm() const;
};

};
#endif
