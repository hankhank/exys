
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <chrono>

#include "cuda.h"

#include "exys.h"
#include "gputer.h"
#include "kernel_gen.h"

// Error Code string definitions here
typedef struct
{
    char const *error_string;
    int  error_id;
} s_CudaErrorStr;

/**
 * Error codes
 */
static s_CudaErrorStr sCudaDrvErrorString[] =
{
    /**
     * The API call returned with no errors. In the case of query calls, this
     * can also mean that the operation being queried is complete (see
     * ::cuEventQuery() and ::cuStreamQuery()).
     */
    { "CUDA_SUCCESS", 0 },

    /**
     * This indicates that one or more of the parameters passed to the API call
     * is not within an acceptable range of values.
     */
    { "CUDA_ERROR_INVALID_VALUE", 1 },

    /**
     * The API call failed because it was unable to allocate enough memory to
     * perform the requested operation.
     */
    { "CUDA_ERROR_OUT_OF_MEMORY", 2 },

    /**
     * This indicates that the CUDA driver has not been initialized with
     * ::cuInit() or that initialization has failed.
     */
    { "CUDA_ERROR_NOT_INITIALIZED", 3 },

    /**
     * This indicates that the CUDA driver is in the process of shutting down.
     */
    { "CUDA_ERROR_DEINITIALIZED", 4 },

    /**
     * This indicates profiling APIs are called while application is running
     * in visual profiler mode. 
    */
    { "CUDA_ERROR_PROFILER_DISABLED", 5 },
    /**
     * This indicates profiling has not been initialized for this context. 
     * Call cuProfilerInitialize() to resolve this. 
    */
    { "CUDA_ERROR_PROFILER_NOT_INITIALIZED", 6 },
    /**
     * This indicates profiler has already been started and probably
     * cuProfilerStart() is incorrectly called.
    */
    { "CUDA_ERROR_PROFILER_ALREADY_STARTED", 7 },
    /**
     * This indicates profiler has already been stopped and probably
     * cuProfilerStop() is incorrectly called.
    */
    { "CUDA_ERROR_PROFILER_ALREADY_STOPPED", 8 },  
    /**
     * This indicates that no CUDA-capable devices were detected by the installed
     * CUDA driver.
     */
    { "CUDA_ERROR_NO_DEVICE (no CUDA-capable devices were detected)", 100 },

    /**
     * This indicates that the device ordinal supplied by the user does not
     * correspond to a valid CUDA device.
     */
    { "CUDA_ERROR_INVALID_DEVICE (device specified is not a valid CUDA device)", 101 },


    /**
     * This indicates that the device kernel image is invalid. This can also
     * indicate an invalid CUDA module.
     */
    { "CUDA_ERROR_INVALID_IMAGE", 200 },

    /**
     * This most frequently indicates that there is no context bound to the
     * current thread. This can also be returned if the context passed to an
     * API call is not a valid handle (such as a context that has had
     * ::cuCtxDestroy() invoked on it). This can also be returned if a user
     * mixes different API versions (i.e. 3010 context with 3020 API calls).
     * See ::cuCtxGetApiVersion() for more details.
     */
    { "CUDA_ERROR_INVALID_CONTEXT", 201 },

    /**
     * This indicated that the context being supplied as a parameter to the
     * API call was already the active context.
     * \deprecated
     * This error return is deprecated as of CUDA 3.2. It is no longer an
     * error to attempt to push the active context via ::cuCtxPushCurrent().
     */
    { "CUDA_ERROR_CONTEXT_ALREADY_CURRENT", 202 },

    /**
     * This indicates that a map or register operation has failed.
     */
    { "CUDA_ERROR_MAP_FAILED", 205 },

    /**
     * This indicates that an unmap or unregister operation has failed.
     */
    { "CUDA_ERROR_UNMAP_FAILED", 206 },

    /**
     * This indicates that the specified array is currently mapped and thus
     * cannot be destroyed.
     */
    { "CUDA_ERROR_ARRAY_IS_MAPPED", 207 },

    /**
     * This indicates that the resource is already mapped.
     */
    { "CUDA_ERROR_ALREADY_MAPPED", 208 },

    /**
     * This indicates that there is no kernel image available that is suitable
     * for the device. This can occur when a user specifies code generation
     * options for a particular CUDA source file that do not include the
     * corresponding device configuration.
     */
    { "CUDA_ERROR_NO_BINARY_FOR_GPU", 209 },

    /**
     * This indicates that a resource has already been acquired.
     */
    { "CUDA_ERROR_ALREADY_ACQUIRED", 210 },

    /**
     * This indicates that a resource is not mapped.
     */
    { "CUDA_ERROR_NOT_MAPPED", 211 },

    /**
     * This indicates that a mapped resource is not available for access as an
     * array.
     */
    { "CUDA_ERROR_NOT_MAPPED_AS_ARRAY", 212 },

    /**
     * This indicates that a mapped resource is not available for access as a
     * pointer.
     */
    { "CUDA_ERROR_NOT_MAPPED_AS_POINTER", 213 },

    /**
     * This indicates that an uncorrectable ECC error was detected during
     * execution.
     */
    { "CUDA_ERROR_ECC_UNCORRECTABLE", 214 },

    /**
     * This indicates that the ::CUlimit passed to the API call is not
     * supported by the active device.
     */
    { "CUDA_ERROR_UNSUPPORTED_LIMIT", 215 },

    /**
     * This indicates that the ::CUcontext passed to the API call can
     * only be bound to a single CPU thread at a time but is already 
     * bound to a CPU thread.
     */
    { "CUDA_ERROR_CONTEXT_ALREADY_IN_USE", 216 },

    /**
     * This indicates that peer access is not supported across the given
     * devices.
     */
    { "CUDA_ERROR_PEER_ACCESS_UNSUPPORTED", 217},

    /**
     * This indicates that the device kernel source is invalid.
     */
    { "CUDA_ERROR_INVALID_SOURCE", 300 },

    /**
     * This indicates that the file specified was not found.
     */
    { "CUDA_ERROR_FILE_NOT_FOUND", 301 },

    /**
     * This indicates that a link to a shared object failed to resolve.
     */
    { "CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND", 302 },

    /**
     * This indicates that initialization of a shared object failed.
     */
    { "CUDA_ERROR_SHARED_OBJECT_INIT_FAILED", 303 },

    /**
     * This indicates that an OS call failed.
     */
    { "CUDA_ERROR_OPERATING_SYSTEM", 304 },


    /**
     * This indicates that a resource handle passed to the API call was not
     * valid. Resource handles are opaque types like ::CUstream and ::CUevent.
     */
    { "CUDA_ERROR_INVALID_HANDLE", 400 },


    /**
     * This indicates that a named symbol was not found. Examples of symbols
     * are global/constant variable names, texture names }, and surface names.
     */
    { "CUDA_ERROR_NOT_FOUND", 500 },


    /**
     * This indicates that asynchronous operations issued previously have not
     * completed yet. This result is not actually an error, but must be indicated
     * differently than ::CUDA_SUCCESS (which indicates completion). Calls that
     * may return this value include ::cuEventQuery() and ::cuStreamQuery().
     */
    { "CUDA_ERROR_NOT_READY", 600 },


    /**
     * An exception occurred on the device while executing a kernel. Common
     * causes include dereferencing an invalid device pointer and accessing
     * out of bounds shared memory. The context cannot be used }, so it must
     * be destroyed (and a new one should be created). All existing device
     * memory allocations from this context are invalid and must be
     * reconstructed if the program is to continue using CUDA.
     */
    { "CUDA_ERROR_LAUNCH_FAILED", 700 },

    /**
     * This indicates that a launch did not occur because it did not have
     * appropriate resources. This error usually indicates that the user has
     * attempted to pass too many arguments to the device kernel, or the
     * kernel launch specifies too many threads for the kernel's register
     * count. Passing arguments of the wrong size (i.e. a 64-bit pointer
     * when a 32-bit int is expected) is equivalent to passing too many
     * arguments and can also result in this error.
     */
    { "CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES", 701 },

    /**
     * This indicates that the device kernel took too long to execute. This can
     * only occur if timeouts are enabled - see the device attribute
     * ::CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT for more information. The
     * context cannot be used (and must be destroyed similar to
     * ::CUDA_ERROR_LAUNCH_FAILED). All existing device memory allocations from
     * this context are invalid and must be reconstructed if the program is to
     * continue using CUDA.
     */
    { "CUDA_ERROR_LAUNCH_TIMEOUT", 702 },

    /**
     * This error indicates a kernel launch that uses an incompatible texturing
     * mode.
     */
    { "CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING", 703 },
    
    /**
     * This error indicates that a call to ::cuCtxEnablePeerAccess() is
     * trying to re-enable peer access to a context which has already
     * had peer access to it enabled.
     */
    { "CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED", 704 },

    /**
     * This error indicates that ::cuCtxDisablePeerAccess() is 
     * trying to disable peer access which has not been enabled yet 
     * via ::cuCtxEnablePeerAccess(). 
     */
    { "CUDA_ERROR_PEER_ACCESS_NOT_ENABLED", 705 },

    /**
     * This error indicates that the primary context for the specified device
     * has already been initialized.
     */
    { "CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE", 708 },

    /**
     * This error indicates that the context current to the calling thread
     * has been destroyed using ::cuCtxDestroy }, or is a primary context which
     * has not yet been initialized.
     */
    { "CUDA_ERROR_CONTEXT_IS_DESTROYED", 709 },

    /**
     * A device-side assert triggered during kernel execution. The context
     * cannot be used anymore, and must be destroyed. All existing device 
     * memory allocations from this context are invalid and must be 
     * reconstructed if the program is to continue using CUDA.
     */
    { "CUDA_ERROR_ASSERT", 710 },

        /**
     * This error indicates that the hardware resources required to enable
     * peer access have been exhausted for one or more of the devices 
     * passed to ::cuCtxEnablePeerAccess().
     */
    { "CUDA_ERROR_TOO_MANY_PEERS", 711 },

    /**
     * This error indicates that the memory range passed to ::cuMemHostRegister()
     * has already been registered.
     */
    { "CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED", 712 },

    /**
     * This error indicates that the pointer passed to ::cuMemHostUnregister()
     * does not correspond to any currently registered memory region.
     */
    { "CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED", 713 },

    /**
     * This error indicates that the attempted operation is not permitted.
     */
    { "CUDA_ERROR_NOT_PERMITTED", 800 },

    /**
     * This error indicates that the attempted operation is not supported
     * on the current system or device.
     */
    { "CUDA_ERROR_NOT_SUPPORTED", 801 },

    /**
     * This indicates that an unknown internal error has occurred.
     */
    { "CUDA_ERROR_UNKNOWN", 999 },
    { NULL, -1 }
};

// This is just a linear search through the array, since the error_id's are not
// always ocurring consecutively
const char * getCudaDrvErrorString(CUresult error_id)
{
    int index = 0;
    while (sCudaDrvErrorString[index].error_id != error_id && 
           sCudaDrvErrorString[index].error_id != -1)
    {
        index++;
    }
    if (sCudaDrvErrorString[index].error_id == error_id)
        return (const char *)sCudaDrvErrorString[index].error_string;
    else
        return (const char *)"CUDA_ERROR not found!";
}


// This will output the proper CUDA error strings in the event that a CUDA
// host call returns an error
#define checkCudaErrors(err)  __checkCudaErrors (err, __FILE__, __LINE__)

// These are the inline versions for all of the SDK helper functions
void __checkCudaErrors(CUresult err, const char *file, const int line) {
  if(CUDA_SUCCESS != err) {
      std::cout << "checkCudeErrors() Driver API error = " << err << "\""
           << getCudaDrvErrorString(err) << "\" from file <" << file
           << ", line " << line << "\n";
    exit(-1);
  }
}

std::string GetDeviceInfo(CUdevice& dev)
{
    std::string output;

#define ADD_TO_OUTPUT(_X) \
    { \
        int pi = 0; \
        cuDeviceGetAttribute(&pi, _X, dev); \
        output += std::string(#_X) + "=" + std::to_string(pi) + "\n"; \
    }

    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_WARP_SIZE)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_CLOCK_RATE)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_GLOBAL_L1_CACHE_SUPPORTED)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_LOCAL_L1_CACHE_SUPPORTED)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MANAGED_MEMORY)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MULTI_GPU_BOARD)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_MULTI_GPU_BOARD_GROUP_ID)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_HOST_NATIVE_ATOMIC_SUPPORTED)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_SINGLE_TO_DOUBLE_PRECISION_PERF_RATIO)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS)
    ADD_TO_OUTPUT(CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS)
    return output;
}

void LinkCudaModules(int blocks, int threads, const std::string& ptx, const std::string& compiledFile, int loopnum, int val, int inputSize,
            int observerSize)
{
    std::cout << "init\n";
  CUdevice device;
  CUmodule cudaModule;
  CUcontext context;
  CUlinkState linker;
  char linkerInfo[1024];
  char linkerErrors[1024];
  int devCount;

  linkerInfo[0] = 0;
  linkerErrors[0] = 0;

  CUjit_option linkerOptions[] = 
  {
    CU_JIT_INFO_LOG_BUFFER,
    CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
    CU_JIT_ERROR_LOG_BUFFER,
    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES,
    CU_JIT_LOG_VERBOSE
  };

  void *linkerOptionValues[] = 
  {
    linkerInfo,
    reinterpret_cast<void*>(1024),
    linkerErrors,
    reinterpret_cast<void*>(1024),
    reinterpret_cast<void*>(1)
  };

  checkCudaErrors(cuInit(0));
  checkCudaErrors(cuDeviceGetCount(&devCount));
  checkCudaErrors(cuDeviceGet(&device, 0));
  checkCudaErrors(cuCtxCreate(&context, 0, device));

  char name[128];
  checkCudaErrors(cuDeviceGetName(name, 128, device));
  std::cout << "Using CUDA Device [0]: " << name << "\n";

  int devMajor, devMinor;
  checkCudaErrors(cuDeviceComputeCapability(&devMajor, &devMinor, device));
  std::cout << "Device Compute Capability: " 
         << devMajor << "." << devMinor << "\n";
  if (devMajor < 2) {
      std::cout << "ERROR: Device 0 is not SM 2.0 or greater\n";
    return ;
  }

  std::cout << GetDeviceInfo(device);

  // Create JIT linker and create final CUBIN
    std::cout << "link create\n";
  checkCudaErrors(cuLinkCreate(sizeof(linkerOptions) / sizeof(linkerOptions[0]), linkerOptions, linkerOptionValues, &linker));
    std::cout << "add data\n";
  checkCudaErrors(cuLinkAddData(linker, CU_JIT_INPUT_PTX, (void*)ptx.c_str(),
                                ptx.size(), "<compiled-ptx>", 0, NULL, NULL));
    std::cout << "add data\n";
  checkCudaErrors(cuLinkAddData(linker, CU_JIT_INPUT_OBJECT, (void*)compiledFile.c_str(),
                                compiledFile.size(), "<precompiled-kernel>", 0, NULL, NULL));
 
  void *cubin;
  size_t cubinSize;
    std::cout << "link complete\n";
  checkCudaErrors(cuLinkComplete(linker, &cubin, &cubinSize));

  std::cout << "Linker Log:\n" << linkerInfo << "\n" << linkerErrors << "\n";
  
  // Create module for object
  checkCudaErrors(cuModuleLoadDataEx(&cudaModule, cubin, 0, 0, 0));

  // Now that the CUBIN is loaded, we can release the linker.
  checkCudaErrors(cuLinkDestroy(linker));

  // Get kernel function
  CUfunction function;
  checkCudaErrors(cuModuleGetFunction(&function, cudaModule, "ExysVal"));

  int numBlocksRunning=threads*blocks;

  // Device data
  CUdeviceptr inputBuf;
  checkCudaErrors(cuMemAlloc(&inputBuf, sizeof(Exys::Point)*inputSize));

  CUdeviceptr observerBuf;
  checkCudaErrors(cuMemAlloc(&observerBuf, sizeof(Exys::Point)*observerSize*numBlocksRunning));
    
  if(val)
  {
      // Kernel parameters
      void *KernelParams[] = { &numBlocksRunning, &inputBuf, &inputSize, &observerBuf, &observerSize};

      std::cout << "Launching kernel\n";

      // Kernel launch - warming
        checkCudaErrors(cuLaunchKernel(function, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));
        checkCudaErrors(cuLaunchKernel(function, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));

        checkCudaErrors(cuLaunchKernel(function, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));
        // Copy into inputs
      auto start = std::chrono::steady_clock::now();
      auto inputs = new Exys::Point[2];
      for(int i = 0; i < 1000; i++)
      {
          inputs[0] = 7+i;
          inputs[1] = 99;
          checkCudaErrors(cuMemcpyHtoD(inputBuf, &inputs[0], sizeof(Exys::Point)*inputSize));
        checkCudaErrors(cuLaunchKernel(function, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));

      }

      std::cout << "Run time " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() 
          << "\n";

      auto output = new Exys::Point[numBlocksRunning];
      checkCudaErrors(cuMemcpyDtoH(&output[0], observerBuf, sizeof(Exys::Point)*observerSize*numBlocksRunning));
      for(int i =0 ; i < numBlocksRunning; ++i)
      {
          std::cout << "Output :" << output[i].mVal << "\n";
      }

  }
  else
  {
      CUfunction simfunction;
      checkCudaErrors(cuModuleGetFunction(&simfunction, cudaModule, "ExysSim"));

      // Device data
      CUdeviceptr inputScratch;
      checkCudaErrors(cuMemAlloc(&inputScratch, sizeof(Exys::Point)*inputSize*numBlocksRunning));

      // Kernel parameters
      void *KernelParams[] = { &numBlocksRunning, &inputBuf, &inputScratch, &inputSize, &observerBuf, &observerSize, &loopnum};

      std::cout << "Launching kernel\n";

      // Kernel launch - warming
        checkCudaErrors(cuLaunchKernel(simfunction, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));
        checkCudaErrors(cuLaunchKernel(simfunction, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));

        checkCudaErrors(cuLaunchKernel(simfunction, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));

        // Copy into inputs
      auto start = std::chrono::steady_clock::now();
      auto inputs = new Exys::Point[2];
      for(int i = 0; i < 1000; i++)
      {
          inputs[0] = 7+i;
          inputs[1] = 99;
          checkCudaErrors(cuMemcpyHtoD(inputBuf, &inputs[0], sizeof(Exys::Point)*inputSize));
        checkCudaErrors(cuLaunchKernel(simfunction, blocks, 1, 1,
                                     threads, 1, 1,
                                     0, NULL, KernelParams, NULL));

      }

      std::cout << "Run time " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() 
          << "\n";

      auto output = new Exys::Point[numBlocksRunning];
      checkCudaErrors(cuMemcpyDtoH(&output[0], observerBuf, sizeof(Exys::Point)*observerSize*numBlocksRunning));
      for(int i =0 ; i < numBlocksRunning; ++i)
      {
          std::cout << "Output :" << output[i].mVal << "\n";
      }
  }
}

int main(int argc, char* argv[])
{
    std::cout << "Testing " << argv[1];
    std::ifstream t(argv[1]);
    std::stringstream buffer;
    buffer << t.rdbuf();
        
    int threads = std::atoi(argv[2]);
    int blocks = std::atoi(argv[3]);
    int loopnum = std::atoi(argv[4]);
    int val = std::atoi(argv[5]);
    int inputsize = std::atoi(argv[6]);
    int outputsize = std::atoi(argv[7]);
    try
    {
        auto gputer = Exys::Gputer::Build(buffer.str());
        std::cout << gputer->GetPTX();
        LinkCudaModules(blocks, threads, gputer->GetPTX(), std::string((const char*)kernel_co, kernel_co_len), loopnum, val,
                    inputsize, outputsize);
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
