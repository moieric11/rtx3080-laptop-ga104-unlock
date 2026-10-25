// Dense GEMMs only. FP8 rows explicitly include storage conversion, not native FP8 MMA.
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#include <cuda_fp8.h>
#include <cublas_v2.h>
#include <cutlass/gemm/device/gemm.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#define CUDA(x) do {auto e=(x);if(e!=cudaSuccess)throw std::runtime_error(std::string(#x)+": "+cudaGetErrorString(e));}while(0)
#define BLAS(x) do {auto e=(x);if(e!=CUBLAS_STATUS_SUCCESS)throw std::runtime_error(std::string(#x)+": cuBLAS "+std::to_string(int(e)));}while(0)
#define CUT(x) do {auto e=(x);if(e!=cutlass::Status::kSuccess)throw std::runtime_error(std::string(#x)+": CUTLASS "+std::to_string(int(e)));}while(0)

struct Buffer {
  void* p=nullptr;
  explicit Buffer(size_t bytes){CUDA(cudaMalloc(&p,bytes));}
  ~Buffer(){if(p)cudaFree(p);}
  Buffer(const Buffer&)=delete;Buffer& operator=(const Buffer&)=delete;
};

// Patterned data, varying in both matrix dimensions. Values are exactly representable
// in FP16/BF16/E4M3/E5M2. Integer inputs use the same integer numerators.
__host__ __device__ int av(int row,int k){return 1+(row*3+k)%7;}
__host__ __device__ int bv(int k,int col){return 1+(k*5+col)%7;}
__global__ void fill(void* out,int n,int side,int kind,bool rowMajorA=false) {
  size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x, count=size_t(n)*n;
  if(i>=count)return;
  int row=int(i%n),col=int(i/n);
  int v=side?bv(row,col):(rowMajorA?av(col,row):av(row,col));float f=v/16.f;
  if(kind==0)((__half*)out)[i]=__float2half(f);
  else if(kind==1)((__nv_bfloat16*)out)[i]=__float2bfloat16(f);
  else if(kind==2)((float*)out)[i]=f;
  else if(kind==3)((int8_t*)out)[i]=int8_t(v);
  else if(kind==4)((uint8_t*)out)[i]=__nv_cvt_float_to_fp8(f,__NV_SATFINITE,__NV_E4M3);
  else if(kind==5)((uint8_t*)out)[i]=__nv_cvt_float_to_fp8(f,__NV_SATFINITE,__NV_E5M2);
}
__global__ void fillInt4(uint8_t* out,int n,bool side) {
  size_t b=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(b>=size_t(n)*n/2)return;
  size_t i=b*2;
  int x=side?bv(int(i%n),int(i/n)):av(int(i/n),int(i%n));
  int y=side?bv(int((i+1)%n),int((i+1)/n)):av(int((i+1)/n),int((i+1)%n));
  out[b]=uint8_t(x|(y<<4));
}
__global__ void decodeFp8(const uint8_t* a,const uint8_t* b,uint16_t* ah,uint16_t* bh,size_t count,bool e5m2) {
  size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;if(i>=count)return;
  auto mode=e5m2?__NV_E5M2:__NV_E4M3;
  ah[i]=__nv_cvt_fp8_to_halfraw(a[i],mode).x;bh[i]=__nv_cvt_fp8_to_halfraw(b[i],mode).x;
}
void initialize(Buffer& b,int n,int side,int kind){fill<<<(size_t(n)*n+255)/256,256>>>(b.p,n,side,kind);CUDA(cudaGetLastError());}

struct Spec {const char* name;int kind;cudaDataType_t input,output;cublasComputeType_t compute;bool fp8=false;};
const Spec specs[]={
  {"FP16_accFP16_outFP16",0,CUDA_R_16F,CUDA_R_16F,CUBLAS_COMPUTE_16F},
  {"FP16_accFP32_outFP32",0,CUDA_R_16F,CUDA_R_32F,CUBLAS_COMPUTE_32F},
  {"FP16_accFP32_outFP16",0,CUDA_R_16F,CUDA_R_16F,CUBLAS_COMPUTE_32F},
  {"BF16_accFP32_outFP32",1,CUDA_R_16BF,CUDA_R_32F,CUBLAS_COMPUTE_32F},
  {"BF16_accFP32_outBF16",1,CUDA_R_16BF,CUDA_R_16BF,CUBLAS_COMPUTE_32F},
  {"TF32_accFP32_outFP32",2,CUDA_R_32F,CUDA_R_32F,CUBLAS_COMPUTE_32F_FAST_TF32},
  {"FP32_strict_accFP32_outFP32",2,CUDA_R_32F,CUDA_R_32F,CUBLAS_COMPUTE_32F_PEDANTIC},
  {"INT8_cuBLAS_accINT32_outINT32",3,CUDA_R_8I,CUDA_R_32I,CUBLAS_COMPUTE_32I},
  {"FP8_E4M3_decodeFP16_accFP32",4,CUDA_R_16F,CUDA_R_32F,CUBLAS_COMPUTE_32F,true},
  {"FP8_E5M2_decodeFP16_accFP32",5,CUDA_R_16F,CUDA_R_32F,CUBLAS_COMPUTE_32F,true}
};

template<class Call> void measure(Call call,int n,const char* name,bool integer,bool converted,double& rate) {
  auto start=std::chrono::steady_clock::now();
  do{call();CUDA(cudaDeviceSynchronize());}while(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<1.0);
  cudaEvent_t begin,end;CUDA(cudaEventCreate(&begin));CUDA(cudaEventCreate(&end));
  std::vector<double> rates,times;
  for(int sample=0;sample<5;sample++) {
    CUDA(cudaEventRecord(begin));for(int j=0;j<4;j++)call();CUDA(cudaEventRecord(end));CUDA(cudaEventSynchronize(end));
    float ms;CUDA(cudaEventElapsedTime(&ms,begin,end));if(!(ms>0))throw std::runtime_error("Invalid timing");
    times.push_back(ms/4);rates.push_back(2.*n*n*n*4/(ms*1e9));
  }
  CUDA(cudaEventDestroy(begin));CUDA(cudaEventDestroy(end));
  auto sorted=rates;std::sort(sorted.begin(),sorted.end());rate=sorted[2];
  std::printf("{\"scenario\":\"%s\",\"n\":%d,\"backend\":\"%s\",\"unit\":\"%s\",\"includes_fp8_decode\":%s,\"median_rate\":%.6f,\"samples_rate\":[",name,n,integer&&(std::string(name).find("INT4")==0||std::string(name).find("CUTLASS")!=std::string::npos)?"CUTLASS":"cuBLAS",integer?"TOPS":"TFLOPS",converted?"true":"false",rate);
  for(int i=0;i<5;i++)std::printf("%s%.6f",i?",":"",rates[i]);
  std::printf("],\"samples_ms_per_gemm\":[");for(int i=0;i<5;i++)std::printf("%s%.6f",i?",":"",times[i]);std::printf("]");
}

struct Check {double error=0;bool passed=true;};
Check check(const Buffer& c,int n,cudaDataType_t type,bool integer,bool rowMajor=false) {
  Check result;const int positions[][2]={{0,0},{1,2},{3,5},{n/2,n/2+1},{n-1,n-1},{n-2,7},{31,63},{127,17},{9,n-1}};
  for(auto& pos:positions) {
    int row=pos[0],col=pos[1];size_t i=rowMajor?size_t(row)*n+col:size_t(col)*n+row;
    double expected=0;for(int k=0;k<n;k++)expected+=av(row,k)*bv(k,col);
    if(!integer)expected/=256.;double value=0;
    if(type==CUDA_R_16F){__half x;CUDA(cudaMemcpy(&x,(char*)c.p+i*2,2,cudaMemcpyDeviceToHost));value=__half2float(x);}
    else if(type==CUDA_R_16BF){__nv_bfloat16 x;CUDA(cudaMemcpy(&x,(char*)c.p+i*2,2,cudaMemcpyDeviceToHost));value=__bfloat162float(x);}
    else if(integer){int32_t x;CUDA(cudaMemcpy(&x,(char*)c.p+i*4,4,cudaMemcpyDeviceToHost));value=x;}
    else {float x;CUDA(cudaMemcpy(&x,(char*)c.p+i*4,4,cudaMemcpyDeviceToHost));value=x;}
    if(!std::isfinite(value))throw std::runtime_error("Non-finite GEMM output");
    double error=std::fabs(value-expected)/std::max(1.,std::fabs(expected));result.error=std::max(result.error,error);
    if(integer?value!=expected:error>(type==CUDA_R_16BF?0.005:type==CUDA_R_16F?0.025:0.0001))
      result.passed=false;
  }
  return result;
}
void finishCheck(Check before,Check after) {
  std::printf(",\"checked_output_positions\":9,\"max_relative_error\":%.9g,\"status\":\"%s\"}\n",
    std::max(before.error,after.error),before.passed&&after.passed?"passed":"accuracy_warning");std::fflush(stdout);
}

void runBlas(cublasHandle_t handle,const Spec& s,int n) {
  bool integer=s.kind==3;size_t count=size_t(n)*n;
  size_t inputBytes=count*(s.fp8||integer?1:s.kind==2?4:2);
  Buffer a(inputBytes),b(inputBytes),c(count*(s.output==CUDA_R_16F||s.output==CUDA_R_16BF?2:4));
  initialize(a,n,0,s.kind);initialize(b,n,1,s.kind);CUDA(cudaMemset(c.p,0,count*(s.output==CUDA_R_16F||s.output==CUDA_R_16BF?2:4)));
  Buffer ah(s.fp8?count*2:2),bh(s.fp8?count*2:2);
  float one=1,zero=0;int ione=1,izero=0;__half hone=__float2half(1),hzero=__float2half(0);
  const void* alpha=integer?(void*)&ione:s.compute==CUBLAS_COMPUTE_16F?(void*)&hone:(void*)&one;
  const void* beta=integer?(void*)&izero:s.compute==CUBLAS_COMPUTE_16F?(void*)&hzero:(void*)&zero;
  auto call=[&](){
    if(s.fp8){decodeFp8<<<(count+255)/256,256>>>((uint8_t*)a.p,(uint8_t*)b.p,(uint16_t*)ah.p,(uint16_t*)bh.p,count,s.kind==5);CUDA(cudaGetLastError());}
    BLAS(cublasGemmEx(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,alpha,s.fp8?ah.p:a.p,s.input,n,s.fp8?bh.p:b.p,s.input,n,beta,c.p,s.output,n,s.compute,CUBLAS_GEMM_DEFAULT));
  };
  call();CUDA(cudaDeviceSynchronize());auto before=check(c,n,s.output,integer);
  double rate;measure(call,n,s.name,integer,s.fp8,rate);
  finishCheck(before,check(c,n,s.output,integer));
}

using Int4Gemm=cutlass::gemm::device::Gemm<cutlass::int4b_t,cutlass::layout::RowMajor,
  cutlass::int4b_t,cutlass::layout::ColumnMajor,int32_t,cutlass::layout::RowMajor,
  int32_t,cutlass::arch::OpClassTensorOp,cutlass::arch::Sm80,
  cutlass::gemm::GemmShape<128,128,128>,cutlass::gemm::GemmShape<64,64,128>,cutlass::gemm::GemmShape<16,8,64>>;
using Int8Gemm=cutlass::gemm::device::Gemm<int8_t,cutlass::layout::RowMajor,
  int8_t,cutlass::layout::ColumnMajor,int32_t,cutlass::layout::RowMajor,
  int32_t,cutlass::arch::OpClassTensorOp,cutlass::arch::Sm80,
  cutlass::gemm::GemmShape<128,128,64>,cutlass::gemm::GemmShape<64,64,64>,cutlass::gemm::GemmShape<16,8,32>>;
template<class Gemm> void runInteger(int n,bool int4) {
  size_t count=size_t(n)*n;Buffer a(int4?count/2:count),b(int4?count/2:count),c(count*4);
  if(int4) {
    fillInt4<<<(count/2+255)/256,256>>>((uint8_t*)a.p,n,false);
    fillInt4<<<(count/2+255)/256,256>>>((uint8_t*)b.p,n,true);
  }else {
    fill<<<(count+255)/256,256>>>(a.p,n,0,3,true);
    fill<<<(count+255)/256,256>>>(b.p,n,1,3);
  }
  CUDA(cudaGetLastError());CUDA(cudaMemset(c.p,0,count*4));
  Gemm op;typename Gemm::Arguments args({n,n,n},{(typename Gemm::ElementA*)a.p,n},{(typename Gemm::ElementB*)b.p,n},
    {(int32_t*)c.p,n},{(int32_t*)c.p,n},{1,0});
  CUT(op.can_implement(args));Buffer workspace(std::max(size_t(1),op.get_workspace_size(args)));CUT(op.initialize(args,workspace.p));
  auto call=[&](){CUT(op());};call();CUDA(cudaDeviceSynchronize());auto before=check(c,n,CUDA_R_32I,true,true);
  double rate;measure(call,n,int4?"INT4_accINT32_outINT32":"INT8_CUTLASS_accINT32_outINT32",true,false,rate);
  finishCheck(before,check(c,n,CUDA_R_32I,true,true));
}

int main(int argc,char** argv) {
  try {
    CUDA(cudaSetDevice(0));cudaDeviceProp p;CUDA(cudaGetDeviceProperties(&p,0));
    if(p.major!=8||p.minor!=6)throw std::runtime_error("Expected SM86 Ampere GPU");
    cublasHandle_t h;BLAS(cublasCreate(&h));BLAS(cublasSetMathMode(h,CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION));
    int rt,driver,blas;CUDA(cudaRuntimeGetVersion(&rt));CUDA(cudaDriverGetVersion(&driver));BLAS(cublasGetVersion(h,&blas));
    std::printf("{\"metadata\":true,\"gpu\":\"%s\",\"runtime\":%d,\"driver_api\":%d,\"cublas\":%d,\"samples\":5,\"batch\":4,\"warmup_seconds_per_case\":1,\"native_fp8\":false,\"cutlass_commit\":\"ad7b2f5e84fcfa124cb02b91d5bd26d238c0459e\"}\n",p.name,rt,driver,blas);std::fflush(stdout);
    std::vector<int> sizes=argc==2&&std::string(argv[1])=="--smoke"?std::vector<int>{256}:std::vector<int>{2048,4096,8192};
    for(int n:sizes){for(auto& s:specs)runBlas(h,s,n);runInteger<Int4Gemm>(n,true);runInteger<Int8Gemm>(n,false);}
    BLAS(cublasDestroy(h));return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
