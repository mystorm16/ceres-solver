// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2022 Google Inc. All rights reserved.
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
// Author: markshachkov@gmail.com (Mark Shachkov)

#include "ceres/power_series_expansion_preconditioner.h"

#include "Eigen/Dense"
#include "ceres/detect_structure.h"
#include "ceres/linear_least_squares_problems.h"
#include "gtest/gtest.h"

namespace ceres::internal {

const double kEpsilon = 1e-14;

class PowerSeriesExpansionPreconditionerTest : public ::testing::Test {
 protected:
  void SetUp() final {
    problem_ = CreateLinearLeastSquaresProblemFromId(5);
    const auto A = down_cast<BlockSparseMatrix*>(problem_->A.get());
    const auto D = problem_->D.get();

    LinearSolver::Options options;
    options.elimination_groups.push_back(problem_->num_eliminate_blocks);
    options.preconditioner_type = SCHUR_POWER_SERIES_EXPANSION;
    isc_ = std::make_unique<ImplicitSchurComplement>(options);
    isc_->Init(*A, D, problem_->b.get());
    num_f_cols_ = isc_->rhs().rows();
    const int num_rows = A->num_rows(), num_cols = A->num_cols(),
              num_e_cols = num_cols - num_f_cols_;

    Matrix A_dense, E, F, DE, DF;
    problem_->A->ToDenseMatrix(&A_dense);
    E = A_dense.leftCols(num_e_cols);
    F = A_dense.rightCols(num_f_cols_);
    DE = VectorRef(D, num_e_cols).asDiagonal();
    DF = VectorRef(D + num_e_cols, num_f_cols_).asDiagonal();

    S_inverse_reference_ =
        (F.transpose() *
             (Matrix::Identity(num_rows, num_rows) -
              E * (E.transpose() * E + DE).inverse() * E.transpose()) *
             F +
         DF)
            .inverse();
  }
  std::unique_ptr<LinearLeastSquaresProblem> problem_;
  std::unique_ptr<ImplicitSchurComplement> isc_;
  int num_f_cols_;
  Matrix S_inverse_reference_;
};

TEST_F(PowerSeriesExpansionPreconditionerTest, SchurInverse) {
  const double spse_tolerance = kEpsilon;
  const int min_iterations = 1;
  const int max_iterations = 50;
  PowerSeriesExpansionPreconditioner preconditioner(
      isc_.get(), spse_tolerance, min_iterations, max_iterations);
  PowerSeriesExpansionPreconditioner preconditioner_fixed_n_iterations(
      isc_.get(), 1 / spse_tolerance, max_iterations, max_iterations);
  PowerSeriesExpansionPreconditioner preconditioner_bad_tolerance(
      isc_.get(), 1 / spse_tolerance, min_iterations, max_iterations);
  PowerSeriesExpansionPreconditioner preconditioner_bad_iterations_limit(
      isc_.get(), spse_tolerance, min_iterations, min_iterations);

  Vector x(num_f_cols_), y(num_f_cols_);
  for (int i = 0; i < num_f_cols_; i++) {
    x.setZero();
    x(i) = 1.0;

    y.setZero();
    preconditioner.RightMultiplyAndAccumulate(x.data(), y.data());
    EXPECT_LT((y - S_inverse_reference_.col(i)).norm(), kEpsilon)
        << "Reference Schur complement inverse and its estimate via "
           "PowerSeriesExpansionPreconditioner differs in "
        << i
        << " column.\nreference : " << S_inverse_reference_.col(i).transpose()
        << "\nestimated: " << y.transpose();

    y.setZero();
    preconditioner_fixed_n_iterations.RightMultiplyAndAccumulate(x.data(),
                                                                 y.data());
    EXPECT_LT((y - S_inverse_reference_.col(i)).norm(), kEpsilon)
        << "Reference Schur complement inverse and its estimate via "
           "PowerSeriesExpansionPreconditioner differs in "
        << i
        << " column.\nreference : " << S_inverse_reference_.col(i).transpose()
        << "\nestimated: " << y.transpose();

    y.setZero();
    preconditioner_bad_tolerance.RightMultiplyAndAccumulate(x.data(), y.data());
    EXPECT_GT((y - S_inverse_reference_.col(i)).norm(), kEpsilon)
        << "Reference Schur complement inverse and its estimate via "
           "PowerSeriesExpansionPreconditioner are too similar, tolerance "
           "stopping criteria failed.";

    y.setZero();
    preconditioner_bad_iterations_limit.RightMultiplyAndAccumulate(x.data(),
                                                                   y.data());
    EXPECT_GT((y - S_inverse_reference_.col(i)).norm(), kEpsilon)
        << "Reference Schur complement inverse and its estimate via "
           "PowerSeriesExpansionPreconditioner are too similar, maximum "
           "iterations stopping criteria failed.";
  }
}

}  // namespace ceres::internal