
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

__device__ volatile uint64_t valRunCount = 0;

extern "C" __global__ void ExysVal(
        volatile uint64_t* inExecId, 
        volatile uint64_t* outExecId, 
        int numBlocksRunning,
        Point* inputs, 
        int inputSize,
        Point* observerScratch,
        int observerScratchSize)
{
    int tid = GetTid();

    if(!RunBlock(numBlocksRunning)) return;

    uint64_t curExecId = *inExecId;

    GetPtrsThisBlock(
            nullptr,
            0,
            &observerScratch,
            observerScratchSize);
    
    // Run this kernel hot hot hot
    while (true)
    {
        while(*inExecId && curExecId != *inExecId);
        
        ExysStabilize(inputs, observerScratch);

        ++curExecId;

        atomicAdd((unsigned long long int *)&valRunCount, 1);

        if(!tid) 
        {
            // all threads should be done and we are the master so update count
            while((valRunCount % numBlocksRunning) != 0);
            *outExecId = *inExecId;
        }
    }
}

extern "C" __global__ void ExysSim(
        volatile uint64_t* inExecId, 
        volatile uint64_t* outExecId, 
        int numBlocksRunning,
        Point* inputs, 
        Point* inputScratch,
        int inputSize,
        Point* observerScratch,
        int observerScratchSize)
{
    int tid = GetTid();

    if(!RunBlock(numBlocksRunning)) return;

    uint64_t curExecId = *inExecId;

    GetPtrsThisBlock(
            &inputScratch,
            inputSize,
            &observerScratch,
            observerScratchSize);
    
    // Run this kernel hot hot hot
    while (true)
    {
        while(*inExecId && curExecId != *inExecId);
        
        // Copy in inputs
        memcpy(inputs, inputScratch, inputSize*sizeof(Exys::Point));

        ExysCaptureState();

        ExysStabilize(inputScratch, observerScratch);

        ExysResetState();

        // Check if our sim job is done

        // Run simfunc to update inputs

        ++curExecId;

        atomicAdd((unsigned long long int *)&valRunCount, 1);

        if(!tid) 
        {
            // all threads should be done and we are the master so update count
            while((valRunCount % numBlocksRunning) != 0);
            *outExecId = *inExecId;
        }
    }

}

}
