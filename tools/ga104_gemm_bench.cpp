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
static float half_float(uint16_t v){unsigned e=(v>>10)&31,m=v&1023;float f=e?std::ldexp(1.f+m/1024.f,(int)e-15):std::ldexp((float)m,-24);return v&0x8000?-f:f;}
int main(){
  CUDA(cudaSetDevice(0));cudaDeviceProp p;CUDA(cudaGetDeviceProperties(&p,0));
  if(p.major!=8 || p.minor!=6 || p.totalGlobalMem<15ULL*1024*1024*1024){std::fprintf(stderr,"Unexpected GPU\n");return 1;}
  cublasHandle_t h;BLAS(cublasCreate(&h));
  BLAS(cublasSetMathMode(h,(cublasMath_t)(CUBLAS_TENSOR_OP_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION)));
  int runtime=0,driver=0,blas=0;CUDA(cudaRuntimeGetVersion(&runtime));CUDA(cudaDriverGetVersion(&driver));BLAS(cublasGetVersion(h,&blas));
  std::printf("{\"gpu\":\"%s\",\"runtime\":%d,\"driver_api\":%d,\"cublas\":%d,\"samples\":9,\"batch\":4,\"warmup_seconds\":1,\"results\":[",p.name,runtime,driver,blas);
  bool first=true;
  for(int n: {2048,4096,8192})for(bool fp16acc: {false,true}){
    size_t count=(size_t)n*n;void *a,*b,*c;CUDA(cudaMalloc(&a,count*2));CUDA(cudaMalloc(&b,count*2));CUDA(cudaMalloc(&c,count*(fp16acc?2:4)));
    CUDA(cudaMemset(a,0x3c,count*2));CUDA(cudaMemset(b,0x3c,count*2));CUDA(cudaMemset(c,0,count*(fp16acc?2:4)));
    float one=1,zero=0;uint16_t half_one=0x3c00,half_zero=0;
    auto gemm=[&](){BLAS(cublasGemmEx(h,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,fp16acc?(void*)&half_one:(void*)&one,
      a,CUDA_R_16F,n,b,CUDA_R_16F,n,fp16acc?(void*)&half_zero:(void*)&zero,c,fp16acc?CUDA_R_16F:CUDA_R_32F,n,
      fp16acc?CUBLAS_COMPUTE_16F:CUBLAS_COMPUTE_32F,CUBLAS_GEMM_DEFAULT_TENSOR_OP));};
    auto start=std::chrono::steady_clock::now();
    do {gemm();CUDA(cudaDeviceSynchronize());}while(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<1.0);
    cudaEvent_t begin,end;CUDA(cudaEventCreate(&begin));CUDA(cudaEventCreate(&end));std::vector<double> rates;
    for(int i=0;i<9;i++){
      CUDA(cudaEventRecord(begin));for(int j=0;j<4;j++)gemm();CUDA(cudaEventRecord(end));CUDA(cudaEventSynchronize(end));
      float ms=0;CUDA(cudaEventElapsedTime(&ms,begin,end));if(ms<=0)return 1;rates.push_back(2.0*n*n*n*4/(ms*1e9));
    }
    double values[2];for(int i=0;i<2;i++){
      size_t offset=i?count-1:0;
      if(fp16acc){uint16_t v;CUDA(cudaMemcpy(&v,(char*)c+offset*2,2,cudaMemcpyDeviceToHost));values[i]=half_float(v);}
      else {float v;CUDA(cudaMemcpy(&v,(char*)c+offset*4,4,cudaMemcpyDeviceToHost));values[i]=v;}
      double expected=n*1.05859375*1.05859375;
      if(!std::isfinite(values[i]) || std::fabs(values[i]-expected)/expected>(fp16acc?0.1:0.005)){
        std::fprintf(stderr,"Bad GEMM output n=%d halfacc=%d value=%g expected=%g\n",n,fp16acc,values[i],expected);return 1;
      }
    }
    std::sort(rates.begin(),rates.end());
    std::printf("%s{\"n\":%d,\"mode\":\"%s\",\"median_tflops\":%.6f,\"first\":%.6f,\"last\":%.6f}",first?"":",",n,fp16acc?"FP16acc":"FP32acc",rates[4],values[0],values[1]);first=false;
    CUDA(cudaEventDestroy(begin));CUDA(cudaEventDestroy(end));CUDA(cudaFree(a));CUDA(cudaFree(b));CUDA(cudaFree(c));
  }
  BLAS(cublasDestroy(h));std::puts("]}");return 0;
}
