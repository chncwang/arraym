#include "wrapper-cublas.h"


#ifdef WITH_CUDA
#pragma warning(disable:4505) // unreferenced local function

#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <helper_cuda.h>

#include "cublas-common.h"

DECLARE_NAMESPACE_NLL

namespace blas
{
namespace detail
{
   /**
   @brief Takes care of the initialization.

   @Note that the first call to init() is slow (about 0.5sec) and could bias benchmarks!
   */
   class CublasConfig
   {
   public:
      CublasConfig()
      {
      }

      void init()
      {
         if (_handle != nullptr)
         {
         } else 
         {
            auto r = cublasCreate(&_handle);
            ensure(r == CUBLAS_STATUS_SUCCESS, "CUBLAS init failed!");
         }
      }

      ~CublasConfig()
      {
         if (_handle != nullptr)
         {
            auto r = cublasDestroy(_handle);
            ensure((r == CUBLAS_STATUS_SUCCESS), "cublas shut down failed!");
         }
      }

      cublasHandle_t handle() const
      {
         return _handle;
      }

   private:
      cublasHandle_t _handle = nullptr;
   };

   CublasConfig config;


   template <class T>
   void free_gpu(T* gpu_ptr)
   {
      if (gpu_ptr != nullptr)
      {
         CHECK_CUDA(cudaFree(gpu_ptr));
      }
   }

   template <class T>
   using gpu_ptr_type = std::unique_ptr<T, void(*)(T*)>;

   template <class T>
   gpu_ptr_type<T> allocate_gpu(int nb_elements)
   {
      T* gpu_ptr = nullptr;
      CHECK_CUDA(cudaMalloc(&gpu_ptr, nb_elements * sizeof(T)));
      return gpu_ptr_type<T>(gpu_ptr, &free_gpu<T>);
   }

   template <class T>
   gpu_ptr_type<T> matrix_cpu_to_gpu(int rows, int cols, const T* A, int lda)
   {
      auto ptr = allocate_gpu<T>(rows * cols);
      CHECK_CUDA(cublasSetMatrix(rows, cols, sizeof(T), A, lda, ptr.get(), rows));
      return std::move(ptr);
   }

   template <class T>
   gpu_ptr_type<T> vector_cpu_to_gpu(int n, const T* A, int stride_a)
   {
      auto ptr = allocate_gpu<T>(n);
      CHECK_CUDA(cublasSetVector(n, sizeof(T), A, stride_a, ptr.get(), 1));
      return std::move(ptr);
   }

   template <class T>
   void matrix_gpu_to_cpu(int rows, int cols, const T* gpu_ptr, int ldgpu, T* A, int lda)
   {
      CHECK_CUDA(cublasGetMatrix(rows, cols, sizeof(T), gpu_ptr, ldgpu, A, lda));
   }

   template <class T>
   void vector_gpu_to_cpu(int n, const T* gpu_ptr, int stride_gpu, T* A, int stride_a)
   {
      CHECK_CUDA(cublasGetVector(n, sizeof(T), gpu_ptr, stride_gpu, A, stride_a));
   }

   template <class T, class F>
   BlasInt gemm_cublas(CBLAS_ORDER matrixOrder, const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_TRANSPOSE TransB, const BlasInt M, const BlasInt N,
      const BlasInt K, const T alpha, const T* A, const BlasInt lda, const T* B, const BlasInt ldb,
      const T beta, T* C, const BlasInt ldc, F f)
   {
      using value_type = T;

      if (matrixOrder == CBLAS_ORDER::CblasRowMajor)
      {
         // TODO should be handle different memory orders? For now, no
         return -1;
      }

      // copy the CPU based matrices to GPU
      gpu_ptr_type<value_type> gpu_ptr_a = (TransA == CBLAS_TRANSPOSE::CblasNoTrans) ? matrix_cpu_to_gpu(M, K, A, lda) : matrix_cpu_to_gpu(K, M, A, lda);
      gpu_ptr_type<value_type> gpu_ptr_b = (TransB == CBLAS_TRANSPOSE::CblasNoTrans) ? matrix_cpu_to_gpu(K, N, B, ldb) : matrix_cpu_to_gpu(N, K, B, ldb);
      auto gpu_ptr_c = matrix_cpu_to_gpu<value_type>(M, N, C, ldc);

      const auto gpu_stride_a = (TransA == CBLAS_TRANSPOSE::CblasNoTrans) ? M : K;
      const auto gpu_stride_b = (TransB == CBLAS_TRANSPOSE::CblasNoTrans) ? K : N;

      // run calculation on the GPU
      auto transa = is_transposed(matrixOrder, TransA);
      auto transb = is_transposed(matrixOrder, TransB);

      const int ld_gpu = M;

      INIT_AND_CHECK_CUDA(f(config.handle(), transa, transb, M, N, K, &alpha, gpu_ptr_a.get(), gpu_stride_a, gpu_ptr_b.get(), gpu_stride_b, &beta, gpu_ptr_c.get(), ld_gpu));

      // get the data back on CPU
      matrix_gpu_to_cpu(M, N, gpu_ptr_c.get(), ld_gpu, C, ldc);
      return 0;
   }

   BlasInt sgemm_cublas(CBLAS_ORDER matrixOrder, const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_TRANSPOSE TransB, const BlasInt M, const BlasInt N,
      const BlasInt K, const BlasReal alpha, const BlasReal* A, const BlasInt lda, const BlasReal* B, const BlasInt ldb,
      const BlasReal beta, BlasReal* C, const BlasInt ldc)
   {
      return gemm_cublas<BlasReal>(matrixOrder, TransA, TransB, M, N, K,
         alpha, A, lda,
         B, ldb,
         beta, C, ldc, &cublasSgemm);
   }

   BlasInt dgemm_cublas(CBLAS_ORDER matrixOrder, const enum CBLAS_TRANSPOSE TransA, const enum CBLAS_TRANSPOSE TransB, const BlasInt M, const BlasInt N,
      const BlasInt K, const BlasDoubleReal alpha, const BlasDoubleReal* A, const BlasInt lda, const BlasDoubleReal* B, const BlasInt ldb,
      const BlasDoubleReal beta, BlasDoubleReal* C, const BlasInt ldc)
   {
      return gemm_cublas<BlasDoubleReal>(matrixOrder, TransA, TransB, M, N, K,
         alpha, A, lda,
         B, ldb,
         beta, C, ldc, &cublasDgemm);
   }

   BlasInt saxpy_cublas(BlasInt N, BlasReal alpha, const BlasReal* x, BlasInt incx, BlasReal* y, BlasInt incy)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, x, incx);
      auto gpu_ptr_y = vector_cpu_to_gpu(N, y, incy);

      INIT_AND_CHECK_CUDA(cublasSaxpy(config.handle(), N, &alpha, gpu_ptr_x.get(), 1,
                                                                  gpu_ptr_y.get(), 1));

      vector_gpu_to_cpu(N, gpu_ptr_y.get(), 1, y, incy);
      return 0;
   }

   BlasInt daxpy_cublas(BlasInt N, BlasDoubleReal alpha, const BlasDoubleReal* x, BlasInt incx, BlasDoubleReal* y, BlasInt incy)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu( N, x, incx );
      auto gpu_ptr_y = vector_cpu_to_gpu( N, y, incy );

      INIT_AND_CHECK_CUDA( cublasDaxpy( config.handle(), N, &alpha, gpu_ptr_x.get(), 1,
         gpu_ptr_y.get(), 1 ) );

      vector_gpu_to_cpu( N, gpu_ptr_y.get(), 1, y, incy );
      return 0;
   }

   BlasInt sscal_cublas(const BlasInt N, const BlasReal alpha, BlasReal* X, const BlasInt incX)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, X, incX);
      INIT_AND_CHECK_CUDA(cublasSscal(config.handle(), N, &alpha, gpu_ptr_x.get(), 1));
      vector_gpu_to_cpu(N, gpu_ptr_x.get(), 1, X, incX);
      return 0;
   }

   BlasInt dscal_cublas(const BlasInt N, const BlasDoubleReal alpha, BlasDoubleReal* X, const BlasInt incX)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, X, incX);
      INIT_AND_CHECK_CUDA(cublasDscal(config.handle(), N, &alpha, gpu_ptr_x.get(), 1));
      vector_gpu_to_cpu(N, gpu_ptr_x.get(), 1, X, incX);
      return 0;
   }

   BlasReal sdot_cublas(const BlasInt N, const BlasReal* x, const BlasInt incX, const BlasReal* y, const BlasInt incY)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, x, incX);
      auto gpu_ptr_y = vector_cpu_to_gpu(N, y, incY);

      BlasReal result = 0;
      INIT_AND_CHECK_CUDA(cublasSdot(config.handle(), N,
         gpu_ptr_x.get(), 1,
         gpu_ptr_y.get(), 1,
         &result));
      return result;
   }

   BlasDoubleReal ddot_cublas(const BlasInt N, const BlasDoubleReal* x, const BlasInt incX, const BlasDoubleReal* y, const BlasInt incY)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, x, incX);
      auto gpu_ptr_y = vector_cpu_to_gpu(N, y, incY);

      BlasDoubleReal result = 0;
      INIT_AND_CHECK_CUDA(cublasDdot(config.handle(), N,
         gpu_ptr_x.get(), 1,
         gpu_ptr_y.get(), 1,
         &result));
      return result;
   }

   BlasReal snrm2_cublas(const BlasInt N, const BlasReal* X, const BlasInt incX)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, X, incX);
      BlasReal result = 0;
      INIT_AND_CHECK_CUDA(cublasSnrm2(config.handle(), N,
         gpu_ptr_x.get(), 1,
         &result));
      return result;
   }

   BlasDoubleReal dnrm2_cublas(const BlasInt N, const BlasDoubleReal* X, const BlasInt incX)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(N, X, incX);
      BlasDoubleReal result = 0;
      INIT_AND_CHECK_CUDA(cublasDnrm2(config.handle(), N,
         gpu_ptr_x.get(), 1,
         &result));
      return result;
   }

   BlasReal sasum_cublas(const BlasInt n, const BlasReal* x, const BlasInt incx)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(n, x, incx);
      BlasReal result = 0;
      INIT_AND_CHECK_CUDA(cublasSasum(config.handle(), n,
         gpu_ptr_x.get(), 1,
         &result));
      return result;
   }

   BlasDoubleReal dasum_cublas(const BlasInt n, const BlasDoubleReal* x, const BlasInt incx)
   {
      auto gpu_ptr_x = vector_cpu_to_gpu(n, x, incx);
      BlasDoubleReal result = 0;
      INIT_AND_CHECK_CUDA(cublasDasum(config.handle(), n,
         gpu_ptr_x.get(), 1,
         &result));
      return result;
   }

   template <class T, class F>
   BlasInt ger_cublas(CBLAS_ORDER matrixOrder, const BlasInt M, const BlasInt N, const T alpha, const T* x, const BlasInt incX,
      const T* y, const BlasInt incY, T* A, const BlasInt lda, F f)
   {
      using value_type = T;

      if (matrixOrder == CBLAS_ORDER::CblasRowMajor)
      {
         // TODO should be handle different memory orders? For now, no
         return -1;
      }

      // copy the CPU based matrices to GPU
      gpu_ptr_type<value_type> gpu_ptr_x = vector_cpu_to_gpu(M, x, incX);
      gpu_ptr_type<value_type> gpu_ptr_y = vector_cpu_to_gpu(N, y, incY);
      auto gpu_ptr_a = matrix_cpu_to_gpu<value_type>(M, N, A, lda);
      const int ld_gpu = M;

      INIT_AND_CHECK_CUDA(f(config.handle(), M, N, &alpha, gpu_ptr_x.get(), 1, gpu_ptr_y.get(), 1, gpu_ptr_a.get(), ld_gpu));

      // get the data back on CPU
      matrix_gpu_to_cpu(M, N, gpu_ptr_a.get(), ld_gpu, A, lda);
      return 0;
   }

   BlasInt sger_cublas(CBLAS_ORDER matrixOrder, const BlasInt M, const BlasInt N, const BlasReal alpha, const BlasReal* x, const BlasInt incX,
      const BlasReal* y, const BlasInt incY, BlasReal* A, const BlasInt lda)
   {
      return ger_cublas(matrixOrder, M, N, alpha, x, incX, y, incY, A, lda, &cublasSger);
   }

   BlasInt dger_cublas(CBLAS_ORDER matrixOrder, const BlasInt M, const BlasInt N, const BlasDoubleReal alpha, const BlasDoubleReal* x, const BlasInt incX,
      const BlasDoubleReal* y, const BlasInt incY, BlasDoubleReal* A, const BlasInt lda)
   {
      return ger_cublas(matrixOrder, M, N, alpha, x, incX, y, incY, A, lda, &cublasDger);
   }
}
}

DECLARE_NAMESPACE_NLL_END

#endif