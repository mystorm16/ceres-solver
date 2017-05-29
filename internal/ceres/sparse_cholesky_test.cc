// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/sparse_cholesky.h"

#include <numeric>
#include <vector>

#include "Eigen/Dense"
#include "Eigen/SparseCore"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/random.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace ceres {
namespace internal {

CompressedRowSparseMatrix* CreateRandomSymmetricPositiveDefiniteMatrix(
    const int num_col_blocks,
    const int min_col_block_size,
    const int max_col_block_size,
    const double block_density,
    const CompressedRowSparseMatrix::StorageType storage_type) {
  // Create a random matrix
  CompressedRowSparseMatrix::RandomMatrixOptions options;
  options.num_col_blocks = num_col_blocks;
  options.min_col_block_size = min_col_block_size;
  options.max_col_block_size = max_col_block_size;

  options.num_row_blocks = 2 * num_col_blocks;
  options.min_row_block_size = 1;
  options.max_row_block_size = max_col_block_size;
  options.block_density = block_density;
  scoped_ptr<CompressedRowSparseMatrix> random_crsm(
      CompressedRowSparseMatrix::CreateRandomMatrix(options));

  // Add a diagonal block sparse matrix to make it full rank.
  Vector diagonal = Vector::Ones(random_crsm->num_cols());
  scoped_ptr<CompressedRowSparseMatrix> block_diagonal(
      CompressedRowSparseMatrix::CreateBlockDiagonalMatrix(
          diagonal.data(), random_crsm->col_blocks()));
  random_crsm->AppendRows(*block_diagonal);

  // Compute output = random_crsm' * random_crsm
  std::vector<int> program;
  CompressedRowSparseMatrix* output =
      CompressedRowSparseMatrix::CreateOuterProductMatrixAndProgram(
          *random_crsm, storage_type, &program);
  CompressedRowSparseMatrix::ComputeOuterProduct(*random_crsm, program, output);
  return output;
}

bool ComputeExpectedSolution(const CompressedRowSparseMatrix& lhs,
                             const Vector& rhs,
                             Vector* solution) {
  Matrix eigen_lhs;
  lhs.ToDenseMatrix(&eigen_lhs);
  if (lhs.storage_type() == CompressedRowSparseMatrix::UPPER_TRIANGULAR) {
    Matrix full_lhs = eigen_lhs.selfadjointView<Eigen::Upper>();
    Eigen::LLT<Matrix, Eigen::Upper> llt =
        eigen_lhs.selfadjointView<Eigen::Upper>().llt();
    if (llt.info() != Eigen::Success) {
      return false;
    }
    *solution = llt.solve(rhs);
    return (llt.info() == Eigen::Success);
  }

  Matrix full_lhs = eigen_lhs.selfadjointView<Eigen::Lower>();
  Eigen::LLT<Matrix, Eigen::Lower> llt =
      eigen_lhs.selfadjointView<Eigen::Lower>().llt();
  if (llt.info() != Eigen::Success) {
    return false;
  }
  *solution = llt.solve(rhs);
  return (llt.info() == Eigen::Success);
}

void SparseCholeskySolverUnitTest(
    const SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type,
    const OrderingType ordering_type,
    const bool use_block_structure,
    const int num_blocks,
    const int min_block_size,
    const int max_block_size,
    const double block_density) {
  scoped_ptr<SparseCholesky> sparse_cholesky(SparseCholesky::Create(
      sparse_linear_algebra_library_type, ordering_type));
  const CompressedRowSparseMatrix::StorageType storage_type =
      sparse_cholesky->StorageType();

  scoped_ptr<CompressedRowSparseMatrix> lhs(
      CreateRandomSymmetricPositiveDefiniteMatrix(num_blocks,
                                                  min_block_size,
                                                  max_block_size,
                                                  block_density,
                                                  storage_type));
  if (!use_block_structure) {
    lhs->mutable_row_blocks()->clear();
    lhs->mutable_col_blocks()->clear();
  }

  Vector rhs = Vector::Random(lhs->num_rows());
  Vector expected(lhs->num_rows());
  Vector actual(lhs->num_rows());

  EXPECT_TRUE(ComputeExpectedSolution(*lhs, rhs, &expected));
  std::string message;
  EXPECT_EQ(sparse_cholesky->FactorAndSolve(
                lhs.get(), rhs.data(), actual.data(), &message),
            LINEAR_SOLVER_SUCCESS);
  Matrix eigen_lhs;
  lhs->ToDenseMatrix(&eigen_lhs);
  EXPECT_NEAR((actual - expected).norm() / actual.norm(),
              0.0,
              std::numeric_limits<double>::epsilon() * 10)
      << "\n"
      << eigen_lhs;
}

#ifdef CERES_USE_CXX11
using std::tuple;
using std::get;
#else
using std::tr1::tuple;
using std::tr1::get;
#endif

typedef tuple<SparseLinearAlgebraLibraryType, OrderingType, bool>
    Param;

std::string ParamInfoToString(testing::TestParamInfo<Param> info) {
  Param param = info.param;
  std::stringstream ss;
  ss << SparseLinearAlgebraLibraryTypeToString(get<0>(param)) << "_"
     << (get<1>(param) == AMD ? "AMD" : "NATURAL") << "_"
     << (get<2>(param) ? "UseBlockStructure" : "NoBlockStructure");
  return ss.str();
}

class SparseCholeskyTest : public ::testing::TestWithParam<Param> {};

TEST_P(SparseCholeskyTest, FactorAndSolve) {
  SetRandomState(2982);
  const int kMinNumBlocks = 1;
  const int kMaxNumBlocks = 10;
  const int kNumTrials = 10;
  const int kMinBlockSize = 1;
  const int kMaxBlockSize = 5;

  for (int num_blocks = kMinNumBlocks; num_blocks < kMaxNumBlocks;
       ++num_blocks) {
    for (int trial = 0; trial < kNumTrials; ++trial) {
      const double block_density = std::max(0.1, RandDouble());
      Param param = GetParam();
      SparseCholeskySolverUnitTest(get<0>(param),
                                   get<1>(param),
                                   get<2>(param),
                                   num_blocks,
                                   kMinBlockSize,
                                   kMaxBlockSize,
                                   block_density);
    }
  }
}

#ifndef CERES_NO_SUITESPARSE
INSTANTIATE_TEST_CASE_P(SuiteSparseCholesky,
                        SparseCholeskyTest,
                        ::testing::Combine(::testing::Values(SUITE_SPARSE),
                                           ::testing::Values(AMD, NATURAL),
                                           ::testing::Values(true, false)),
                        ParamInfoToString);
#endif

#ifndef CERES_NO_CXSPARSE
INSTANTIATE_TEST_CASE_P(CXSparseCholesky,
                        SparseCholeskyTest,
                        ::testing::Combine(::testing::Values(CX_SPARSE),
                                           ::testing::Values(AMD, NATURAL),
                                           ::testing::Values(true, false)),
                        ParamInfoToString);
#endif

#ifdef CERES_USE_EIGEN_SPARSE
INSTANTIATE_TEST_CASE_P(EigenSparseCholesky,
                        SparseCholeskyTest,
                        ::testing::Combine(::testing::Values(EIGEN_SPARSE),
                                           ::testing::Values(AMD, NATURAL),
                                           ::testing::Values(true, false)),
                        ParamInfoToString);
#endif

}  // namespace internal
}  // namespace ceres
