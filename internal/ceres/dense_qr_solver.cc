// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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

#include "ceres/dense_qr_solver.h"

#include <cstddef>

#include "Eigen/Dense"
#include "ceres/dense_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/linear_solver.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace internal {

DenseQRSolver::DenseQRSolver(const LinearSolver::Options& options)
    : options_(options) {}

LinearSolver::Summary DenseQRSolver::SolveImpl(
    DenseSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventTimer event_timer(&execution_summary_.times);

  const int num_rows = A->num_rows();
  const int num_cols = A->num_cols();

  if (per_solve_options.D != NULL) {
    // Temporarily append a diagonal block to the A matrix, but undo
    // it before returning the matrix to the user.
    A->AppendDiagonal(per_solve_options.D);
  }

  // rhs = [b;0] to account for the additional rows in the lhs.
  const int augmented_num_rows = num_rows + ((per_solve_options.D != NULL) ? num_cols : 0);
  if (rhs_.rows() != augmented_num_rows) {
    rhs_.resize(augmented_num_rows);
    rhs_.setZero();
  }
  rhs_.head(num_rows) = ConstVectorRef(b, num_rows);
  event_timer.AddRelativeEvent("LinearSolver::Setup");

  // Solve the system.
  VectorRef(x, num_cols) = A->matrix().colPivHouseholderQr().solve(rhs_);
  event_timer.AddRelativeEvent("LinearSolver::Solve");

  if (per_solve_options.D != NULL) {
    // Undo the modifications to the matrix A.
    A->RemoveDiagonal();
  }

  // We always succeed, since the QR solver returns the best solution
  // it can. It is the job of the caller to determine if the solution
  // is good enough or not.
  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  summary.termination_type = TOLERANCE;
  event_timer.AddRelativeEvent("LinearSolver::TearDown");

  event_timer.AddAbsoluteEvent("LinearSolver::Total");

  return summary;
}

}   // namespace internal
}   // namespace ceres
