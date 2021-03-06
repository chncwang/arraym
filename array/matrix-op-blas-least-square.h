#pragma once

DECLARE_NAMESPACE_NLL

/**
@brief compute the least square solution min_x ||B - A * X||_2   using QR decomposition

if a_transposed == true, solves min_x ||B - A^t * X||_2 instead
@return B
*/
template <class T, class Config>
Matrix_BlasEnabled<T, 2, Config> least_square(const Array<T, 2, Config>& a, const Array<T, 2, Config>& b, bool a_transposed = false)
{
   using matrix_type         = Matrix_BlasEnabled<T, 2, Config>;
   const auto memory_order_a = getMatrixMemoryOrder(a);
   const auto memory_order_b = getMatrixMemoryOrder(b);
   ensure(memory_order_a == memory_order_b, "matrix must have the same memory order");

   matrix_type a_cpy = a; // will have the result of the factorization
   matrix_type b_cpy = b;

   const blas::BlasInt lda = leading_dimension<T, Config>(a_cpy);
   const blas::BlasInt ldb = leading_dimension<T, Config>(b_cpy);

   const auto m    = static_cast<blas::BlasInt>(a.rows());
   const auto n    = static_cast<blas::BlasInt>(a.columns());
   const auto nrhs = static_cast<blas::BlasInt>(b.columns());

   const auto info = blas::gels<T>(memory_order_a, a_transposed ? 'T' : 'N', m, n, nrhs, &a_cpy(0, 0), lda, &b_cpy(0, 0), ldb);
   ensure(info == 0, "blas::gels<T> failed!");

   auto result_b_based = b_cpy.subarray(typename matrix_type::index_type(0, 0), typename matrix_type::index_type(n - 1, nrhs - 1));
   return result_b_based;
}

DECLARE_NAMESPACE_NLL_END
