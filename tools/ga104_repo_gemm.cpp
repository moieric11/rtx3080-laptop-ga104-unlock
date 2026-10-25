#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

#define CUDA(call) do {cudaError_t r=(call);if(r!=cudaSuccess){std::fprintf(stderr,"CUDA %s: %s\n",#call,cudaGetErrorString(r));std::exit(1);}}while(0)
#define BLAS(call) do {cublasStatus_t r=(call);if(r!=CUBLAS_STATUS_SUCCESS){std::fprintf(stderr,"cuBLAS %s: %d\n",#call,(int)r);std::exit(1);}}while(0)

// Reproduces the published parameters, not unpublished benchmark source.
// Additional choices: NN, alpha=1, beta=0, one-second warmup per mode,
// ten GEMMs per event-timed sample; reduced-precision reductions disallowed.
int main() {
  constexpr int n=8192, batch=10, samples=5;
  constexpr size_t count=size_t(n)*n;
  CUDA(cudaSetDevice(0));cudaDeviceProp prop;CUDA(cudaGetDeviceProperties(&prop,0));
  if(prop.major!=8 || prop.minor!=6){std::fprintf(stderr,"Expected Ampere SM86 GPU\n");return 1;}
  cublasHandle_t h;BLAS(cublasCreate(&h));
  BLAS(cublasSetMathMode(h,CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION));
  int runtime=0,driver=0,blas=0;
  CUDA(cudaRuntimeGetVersion(&runtime));CUDA(cudaDriverGetVersion(&driver));BLAS(cublasGetVersion(h,&blas));
  std::printf("{\"gpu\":\"%s\",\"runtime\":%d,\"driver_api\":%d,\"cublas\":%d,\"n\":%d,\"samples\":%d,\"batch\":%d,\"warmup_seconds_per_mode\":1,\"output\":\"FP32\",\"results\":[",prop.name,runtime,driver,blas,n,samples,batch);
  for(int mode=0;mode<3;mode++) {
    const char* name=mode==0?"FP16":mode==1?"BF16":"TF32";
    cudaDataType_t type=mode==0?CUDA_R_16F:mode==1?CUDA_R_16BF:CUDA_R_32F;
    size_t bytes=count*(mode==2?4:2);void *a,*b,*c;
    CUDA(cudaMalloc(&a,bytes));CUDA(cudaMalloc(&b,bytes));CUDA(cudaMalloc(&c,count*4));
    // Exactly representable in all three formats; expected output is 8160.
    if(mode==2) {
      std::vector<float> host(count,1.0625f);CUDA(cudaMemcpy(a,host.data(),bytes,cudaMemcpyHostToDevice));
      std::fill(host.begin(),host.end(),0.9375f);CUDA(cudaMemcpy(b,host.data(),bytes,cudaMemcpyHostToDevice));
    } else {
      std::vector<uint16_t> host(count,mode==0?0x3c40:0x3f88);
      CUDA(cudaMemcpy(a,host.data(),bytes,cudaMemcpyHostToDevice));
      std::fill(host.begin(),host.end(),uint16_t(mode==0?0x3b80:0x3f70));
      CUDA(cudaMemcpy(b,host.data(),bytes,cudaMemcpyHostToDevice));
    }
    CUDA(cudaMemset(c,0,count*4));float one=1,zero=0;
    auto gemm=[&](){BLAS(cublasGemmEx(h,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&one,a,type,n,b,type,n,&zero,c,CUDA_R_32F,n,
      mode==2?CUBLAS_COMPUTE_32F_FAST_TF32:CUBLAS_COMPUTE_32F,CUBLAS_GEMM_DEFAULT_TENSOR_OP));};
    auto start=std::chrono::steady_clock::now();
    do {gemm();CUDA(cudaDeviceSynchronize());}while(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<1);
    cudaEvent_t begin,end;CUDA(cudaEventCreate(&begin));CUDA(cudaEventCreate(&end));
    std::vector<double> rates,times;
    for(int i=0;i<samples;i++) {
      CUDA(cudaEventRecord(begin));for(int j=0;j<batch;j++)gemm();CUDA(cudaEventRecord(end));CUDA(cudaEventSynchronize(end));
      float ms=0;CUDA(cudaEventElapsedTime(&ms,begin,end));if(ms<=0)return 1;
      times.push_back(ms/batch);rates.push_back(2.0*n*n*n*batch/(ms*1e9));
    }
    double maxError=0;const size_t positions[]={0,size_t(n-1),count/2,count-1};
    for(size_t index:positions) {
      float value;CUDA(cudaMemcpy(&value,(char*)c+index*4,4,cudaMemcpyDeviceToHost));
      if(!std::isfinite(value)){std::fprintf(stderr,"Non-finite result\n");return 1;}
      maxError=std::max(maxError,std::fabs(double(value)-8160.0)/8160.0);
    }
    if(maxError>0.001){std::fprintf(stderr,"Bad %s output, relative error %g\n",name,maxError);return 1;}
    auto sorted=rates;std::sort(sorted.begin(),sorted.end());
    std::printf("%s{\"input\":\"%s\",\"median_tflops\":%.6f,\"max_sampled_relative_error\":%.9g,\"samples_tflops\":[",mode?",":"",name,sorted[2],maxError);
    for(int i=0;i<samples;i++)std::printf("%s%.6f",i?",":"",rates[i]);
    std::printf("],\"samples_ms_per_gemm\":[");for(int i=0;i<samples;i++)std::printf("%s%.6f",i?",":"",times[i]);std::printf("]}");
    CUDA(cudaEventDestroy(begin));CUDA(cudaEventDestroy(end));CUDA(cudaFree(a));CUDA(cudaFree(b));CUDA(cudaFree(c));
  }
  BLAS(cublasDestroy(h));std::puts("]}");return 0;
}
