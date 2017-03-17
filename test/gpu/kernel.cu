
#include "exys.h"

// Functions defined in the jitted code
namespace Exys {
 
extern "C" __device__ void (ExysStabilize)(Point* inputs, Point* observers);
extern "C" __device__ void (ExysCaptureState)();
extern "C" __device__ void (ExysResetState)();

__device__ int GetTid()
{
    return threadIdx.x + blockIdx.x * blockDim.x; 
}

__device__ bool RunBlock(int blocksInUse)
{
    return (GetTid() < blocksInUse);
}

__device__ void GetPtrsThisBlock(
        Point** inputScratch,
        int inputSize,
        Point** observerScratch,
        int observerScratchSize)
{
    int tid = GetTid();

    // Point to my memory
    if(inputScratch)    *inputScratch += inputSize*tid;
    if(observerScratch) *observerScratch += observerScratchSize*tid;
}

extern "C" __global__ void ExysVal(
        int numBlocksRunning,
        Point* inputs, 
        int inputSize,
        Point* observerScratch,
        int observerScratchSize)
{
    int tid = GetTid();

    if(!RunBlock(numBlocksRunning)) return;

    GetPtrsThisBlock(
            nullptr,
            0,
            &observerScratch,
            observerScratchSize);
    
    ExysStabilize(inputs, observerScratch);
}

extern "C" __global__ void ExysSim(
        int numBlocksRunning,
        Point* inputs, 
        Point* inputScratch,
        int inputSize,
        Point* observerScratch,
        int observerScratchSize,
        int sims)
{
    int tid = GetTid();

    if(!RunBlock(numBlocksRunning)) return;

    GetPtrsThisBlock(
            &inputScratch,
            inputSize,
            &observerScratch,
            observerScratchSize);
    
    // Copy in inputs
    memcpy(inputs, inputScratch, inputSize*sizeof(Exys::Point));
    
    for (int i = 0; i < sims; ++i)
    {
        ExysCaptureState();

        ExysStabilize(inputScratch, observerScratch);

        ExysResetState();
    }
}

}
