#include <cuda_runtime.h>
#include <cstdio>
int main(){
  int count=0;cudaError_t e=cudaGetDeviceCount(&count);
  if(e!=cudaSuccess){std::fprintf(stderr,"%s\n",cudaGetErrorString(e));return 1;}
  std::printf("{\"source\":\"CUDA device properties; not a GPU register dump\",\"devices\":[");
  for(int i=0;i<count;i++){
    cudaDeviceProp p;e=cudaGetDeviceProperties(&p,i);
    if(e!=cudaSuccess)return 1;
    std::printf("%s{\"index\":%d,\"name\":\"%s\",\"cc_major\":%d,\"cc_minor\":%d,\"sm_count\":%d,"
      "\"global_memory_bytes\":%llu,\"memory_bus_bits\":%d,\"l2_bytes\":%d,\"async_engine_count\":%d,"
      "\"max_threads_per_sm\":%d,\"shared_memory_per_sm\":%llu}",i?",":"",i,p.name,p.major,p.minor,p.multiProcessorCount,
      (unsigned long long)p.totalGlobalMem,p.memoryBusWidth,p.l2CacheSize,p.asyncEngineCount,p.maxThreadsPerMultiProcessor,
      (unsigned long long)p.sharedMemPerMultiprocessor);
  }
  std::puts("]}");return 0;
}
