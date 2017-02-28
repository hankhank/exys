
#include "exys.h"

// Functions defined in the jitted code
namespace Exys {

__device__ void (ExysStabilize)(Point* inputs, Point* observers);
__device__ void (ExysCaptureState)();
__device__ void (ExysResetState)();

__global__ void ExysSim(
        int numSims,
        Point* inputs, 
        Point* inputScratch,
        int inputSize,
        Point* observerScratch,
        int observerScratchSize,
        uint64_t execId, 
        uint64_t* execIdScratch)
{
    int tid = blockIdx.x;
    if(tid > numSims)
    {
        // no sim for me
        return;
    }
    
    // Point to my memory
    inputScratch += inputSize*tid;
    observerScratch += observerScratchSize*tid;
    execIdScratch += tid;

    // Copy in inputs
    memcpy(inputs, inputScratch, inputSize*sizeof(double));

    ExysCaptureState();

    ExysStabilize(inputScratch, observerScratch);

    ExysResetState();
    
    // Check if our sim job is done

    // Run simfunc to update inputs

}

}
