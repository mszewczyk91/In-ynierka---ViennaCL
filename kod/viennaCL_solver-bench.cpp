#ifndef NDEBUG
 #define NDEBUG
#endif

#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/operation_sparse.hpp>

#define VIENNACL_WITH_UBLAS 1

#include "viennacl/scalar.hpp"
#include "viennacl/vector.hpp"
#include "viennacl/coordinate_matrix.hpp"
#include "viennacl/compressed_matrix.hpp"
#include "viennacl/matrix.hpp"
#include "viennacl/ell_matrix.hpp"
#include "viennacl/hyb_matrix.hpp"

#include "viennacl/linalg/cg.hpp"
#include "viennacl/linalg/bicgstab.hpp"
#include "viennacl/linalg/gmres.hpp"

#include "viennacl/linalg/direct_solve.hpp"
#include "viennacl/linalg/lu.hpp"
#include "viennacl/linalg/ilu.hpp"
#include "viennacl/linalg/ichol.hpp"
#include "viennacl/linalg/jacobi_precond.hpp"
#include "viennacl/linalg/row_scaling.hpp"
#include "viennacl/matrix.hpp"
#include "viennacl/matrix_proxy.hpp"

#include "viennacl/linalg/prod.hpp"
#include "viennacl/linalg/direct_solve.hpp"

#ifdef VIENNACL_WITH_OPENCL
  #include "viennacl/linalg/mixed_precision_cg.hpp"
#endif  

#include "viennacl/io/matrix_market.hpp"


#include <iostream>
#include <vector>
#include "benchmark-utils.hpp"
#include "io.hpp"


using namespace boost::numeric;
using namespace viennacl::linalg;
#define BENCHMARK_RUNS          1
template <typename ScalarType>
ScalarType diff_inf(ublas::vector<ScalarType> & v1, viennacl::vector<ScalarType> & v2)
{
   ublas::vector<ScalarType> v2_cpu(v2.size());
   viennacl::copy(v2.begin(), v2.end(), v2_cpu.begin());

   for (unsigned int i=0;i<v1.size(); ++i)
   {
      if ( std::max( fabs(v2_cpu[i]), fabs(v1[i]) ) > 0 )
         v2_cpu[i] = fabs(v2_cpu[i] - v1[i]) / std::max( fabs(v2_cpu[i]), fabs(v1[i]) );
      else
         v2_cpu[i] = 0.0;
   }

   return norm_inf(v2_cpu);
}

template <typename ScalarType>
ScalarType diff_2(ublas::vector<ScalarType> & v1, viennacl::vector<ScalarType> & v2)
{
   ublas::vector<ScalarType> v2_cpu(v2.size());
   viennacl::copy(v2.begin(), v2.end(), v2_cpu.begin());

   return norm_2(v1 - v2_cpu) / norm_2(v1);
}

template <typename MatrixType, typename VectorType, typename SolverTag, typename PrecondTag>
void run_solver(MatrixType const & matrix, VectorType const & rhs, VectorType const & ref_result, SolverTag const & solver, PrecondTag const & precond, long ops)
{
  Timer timer;
  VectorType result(rhs);
  VectorType residual(rhs);
  viennacl::backend::finish();
  
  timer.start();
  for (int runs=0; runs<BENCHMARK_RUNS; ++runs)
  {
    result = viennacl::linalg::solve(matrix, rhs, solver, precond);
  }
  viennacl::backend::finish();
  double exec_time = timer.get();
  std::cout << "Exec. time: " << exec_time << std::endl;
  std::cout << "Est. "; printOps(ops, exec_time / BENCHMARK_RUNS);
  residual -= viennacl::linalg::prod(matrix, result);
  std::cout << "Relative residual: " << viennacl::linalg::norm_2(residual) / viennacl::linalg::norm_2(rhs) << std::endl;
  std::cout << "Estimated rel. residual: " << solver.error() << std::endl;
  std::cout << "Iterations: " << solver.iters() << std::endl;
  result -= ref_result;
  std::cout << "Relative deviation from result: " << viennacl::linalg::norm_2(result) / viennacl::linalg::norm_2(ref_result) << std::endl;
}

template <typename MatrixType, typename VectorType, typename SolverTag>
void run_direct(MatrixType const & matrix, VectorType const & rhs, VectorType const & ref_result, SolverTag const & solver)
{
  Timer timer;
  VectorType result(rhs);
  VectorType residual(rhs);
  viennacl::backend::finish();
  
  timer.start();
  for (int runs=0; runs<BENCHMARK_RUNS; ++runs)
  {
    result = viennacl::linalg::solve(matrix, rhs, solver);
  }
  viennacl::backend::finish();
  double exec_time = timer.get();
  std::cout << "Exec. time: " << exec_time << std::endl;
  residual -= viennacl::linalg::prod(matrix, result);
  std::cout << "Relative residual: " << viennacl::linalg::norm_2(residual) / viennacl::linalg::norm_2(rhs) << std::endl;
  result -= ref_result;
  std::cout << "Relative deviation from result: " << viennacl::linalg::norm_2(result) / viennacl::linalg::norm_2(ref_result) << std::endl;
}


template<typename ScalarType>
int run_benchmark()
{
  
  Timer timer;
  double exec_time;
   
  ScalarType std_factor1 = static_cast<ScalarType>(3.1415);
  ScalarType std_factor2 = static_cast<ScalarType>(42.0);
  viennacl::scalar<ScalarType> vcl_factor1(std_factor1);
  viennacl::scalar<ScalarType> vcl_factor2(std_factor2);
  
  ublas::vector<ScalarType> ublas_vec1;
  ublas::vector<ScalarType> ublas_vec2;
  ublas::vector<ScalarType> ublas_result;
  unsigned int solver_iters = 1;
  unsigned int solver_krylov_dim = 20;
  double solver_tolerance = 1e-6;

  if (!readVectorFromFile<ScalarType>("rhs.txt", ublas_vec1))
  {
    std::cout << "Error reading RHS file" << std::endl;
    return 0;
  }
  std::cout << "done reading rhs" << std::endl;
  ublas_vec2 = ublas_vec1;
  if (!readVectorFromFile<ScalarType>("result.txt", ublas_result))
  {
    std::cout << "Error reading result file" << std::endl;
    return 0;
  }
  std::cout << "done reading result" << std::endl;
  
  viennacl::compressed_matrix<ScalarType> vcl_compressed_matrix(ublas_vec1.size(), ublas_vec1.size());
  viennacl::coordinate_matrix<ScalarType> vcl_coordinate_matrix(ublas_vec1.size(), ublas_vec1.size());
  viennacl::ell_matrix<ScalarType> vcl_ell_matrix;
  viennacl::hyb_matrix<ScalarType> vcl_hyb_matrix;
  viennacl::matrix<ScalarType> vcl_matrix(ublas_vec1.size(), ublas_vec1.size());

  viennacl::vector<ScalarType> vcl_direct(ublas_vec1.size());
  viennacl::vector<ScalarType> vcl_vec1(ublas_vec1.size());
  viennacl::vector<ScalarType> vcl_vec2(ublas_vec1.size()); 
  viennacl::vector<ScalarType> vcl_result(ublas_vec1.size()); 
  

  ublas::compressed_matrix<ScalarType> ublas_matrix;
  if (!viennacl::io::read_matrix_market_file(ublas_matrix, "matrix.mtx"))
  {
    std::cout << "Error reading Matrix file" << std::endl;
    return EXIT_FAILURE;
  }
  //unsigned int cg_mat_size = cg_mat.size(); 
  std::cout << "done reading matrix" << std::endl;
  
  //cpu to gpu:
  viennacl::copy(ublas_matrix, vcl_compressed_matrix);
  viennacl::copy(ublas_matrix, vcl_coordinate_matrix);
  viennacl::copy(ublas_matrix, vcl_ell_matrix);
  viennacl::copy(ublas_matrix, vcl_hyb_matrix);
  viennacl::copy(ublas_matrix, vcl_matrix);
  viennacl::copy(ublas_vec1, vcl_vec1);
  viennacl::copy(ublas_vec1, vcl_direct);
  viennacl::copy(ublas_vec2, vcl_vec2);
  viennacl::copy(ublas_result, vcl_result);

  ///////////////////////////////////////////////////////////////////////////////
  //////////////////////              CG solver                //////////////////
  ///////////////////////////////////////////////////////////////////////////////

  long cg_ops = static_cast<long>(solver_iters * (ublas_matrix.nnz() + 6 * ublas_vec2.size()));
  
  viennacl::linalg::cg_tag cg_solver(solver_tolerance, solver_iters);
  
  std::cout << "------- CG solver (no preconditioner) using ublas ----------" << std::endl;
  run_solver(ublas_matrix, ublas_vec2, ublas_result, cg_solver, viennacl::linalg::no_precond(), cg_ops);

  std::cout << "------- CG solver (no preconditioner) via ViennaCL, compressed_matrix ----------" << std::endl;
  run_solver(vcl_compressed_matrix, vcl_vec2, vcl_result, cg_solver, viennacl::linalg::no_precond(), cg_ops);

   ///////////////////////////////////////////////////////////////////////////////
  //////////////////////           BiCGStab solver             //////////////////
  ///////////////////////////////////////////////////////////////////////////////
  
  long bicgstab_ops = static_cast<long>(solver_iters * (2 * ublas_matrix.nnz() + 13 * ublas_vec2.size()));
  
  viennacl::linalg::bicgstab_tag bicgstab_solver(solver_tolerance, solver_iters);

  std::cout << "------- BiCGStab solver (no preconditioner) using ublas ----------" << std::endl;
  run_solver(ublas_matrix, ublas_vec2, ublas_result, bicgstab_solver, viennacl::linalg::no_precond(), bicgstab_ops);
  
  std::cout << "------- BiCGStab solver (no preconditioner) via ViennaCL, compressed_matrix ----------" << std::endl;
  run_solver(vcl_compressed_matrix, vcl_vec2, vcl_result, bicgstab_solver, viennacl::linalg::no_precond(), bicgstab_ops);

  ///////////////////////////////////////////////////////////////////////////////
  ///////////////////////            GMRES solver             ///////////////////
  ///////////////////////////////////////////////////////////////////////////////
  
  long gmres_ops = static_cast<long>(solver_iters * (ublas_matrix.nnz() + (solver_iters * 2 + 7) * ublas_vec2.size()));
  
  viennacl::linalg::gmres_tag gmres_solver(solver_tolerance, solver_iters, solver_krylov_dim);
  
  std::cout << "------- GMRES solver (no preconditioner) using ublas ----------" << std::endl;
  run_solver(ublas_matrix, ublas_vec2, ublas_result, gmres_solver, viennacl::linalg::no_precond(), gmres_ops);
  
  std::cout << "------- GMRES solver (no preconditioner) via ViennaCL, compressed_matrix ----------" << std::endl;
  run_solver(vcl_compressed_matrix, vcl_vec2, vcl_result, gmres_solver, viennacl::linalg::no_precond(), gmres_ops);

  
  ///////////////////////////////////////////////////////////////////////////////
  ///////////////////////           DIRECT SOLVERS            ///////////////////
  ///////////////////////////////////////////////////////////////////////////////

  std::cout << "------- LU UpperTag(no preconditioner) via ViennaCL, matrix ----------" << exec_time << std::endl;
  run_direct(vcl_matrix,vcl_direct,vcl_result,viennacl::linalg::upper_tag());

  std::cout << "------- LU LowerTag(no preconditioner) via ViennaCL, matrix ----------" << exec_time << std::endl;
  run_direct(vcl_matrix,vcl_direct,vcl_result,viennacl::linalg::lower_tag());


  return EXIT_SUCCESS;
}

int main()
{
  std::cout << std::endl;
  std::cout << "----------------------------------------------" << std::endl;
  std::cout << "               Device Info" << std::endl;
  std::cout << "----------------------------------------------" << std::endl;
  
#ifdef VIENNACL_WITH_OPENCL
  std::cout << viennacl::ocl::current_device().info() << std::endl;
#endif
   

  std::cout << std::endl;
  std::cout << "----------------------------------------------" << std::endl;
  std::cout << "----------------------------------------------" << std::endl;
  std::cout << "## Benchmark :: Solver" << std::endl;
  std::cout << "----------------------------------------------" << std::endl;
  std::cout << std::endl;
  std::cout << "   -------------------------------" << std::endl;
  std::cout << "   # benchmarking single-precision" << std::endl;
  std::cout << "   -------------------------------" << std::endl;
  run_benchmark<float>();

  return 0;
}

