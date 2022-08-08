// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
//
// Preconditioned Conjugate Gradients based solver for positive
// semidefinite linear systems.

#ifndef CERES_INTERNAL_CONJUGATE_GRADIENTS_SOLVER_H_
#define CERES_INTERNAL_CONJUGATE_GRADIENTS_SOLVER_H_

#include <cmath>
#include <cstddef>
#include <utility>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/linear_operator.h"
#include "ceres/linear_solver.h"
#include "ceres/stringprintf.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

class LinearOperatorToEigenVectorAdapter {
 public:
  LinearOperatorToEigenVectorAdapter(LinearOperator& linear_operator)
      : linear_operator_(linear_operator) {}
  void RightMultiply(const Vector& x, Vector& y) {
    linear_operator_.RightMultiply(x.data(), y.data());
  }
  int num_rows() const { return linear_operator_.num_rows(); }
  int num_cols() const { return linear_operator_.num_cols(); }

 private:
  LinearOperator& linear_operator_;
};

// This function implements the now classical Conjugate Gradients
// algorithm of Hestenes & Stiefel for solving positive semidefinite
// linear systems. Optionally it can use a preconditioner also to
// reduce the condition number of the linear system and improve the
// convergence rate. Modern references for Conjugate Gradients are the
// books by Yousef Saad and Trefethen & Bau. This implementation of CG
// has been augmented with additional termination tests that are
// needed for forcing early termination when used as part of an
// inexact Newton solver.
//
// For more details see the documentation for
// LinearSolver::PerSolveOptions::r_tolerance and
// LinearSolver::PerSolveOptions::q_tolerance in linear_solver.h.
struct ConjugateGradientsSolverOptions {
  int min_num_iterations;
  int max_num_iterations;
  int residual_reset_period;
  double r_tolerance;
  double q_tolerance;
};

template <typename LinearOperatorType, typename DenseVectorType>
LinearSolver::Summary ConjugateGradientsSolver(
    const ConjugateGradientsSolverOptions options,
    LinearOperatorType& lhs,
    const DenseVectorType& rhs,
    LinearOperatorType& preconditioner,
    DenseVectorType scratch[4],
    DenseVectorType& solution) {
  CHECK_EQ(lhs.num_rows(), lhs.num_cols());

  auto IsZeroOrInfinity = [](double x) {
    return ((x == 0.0) || std::isinf(x));
  };

  const int num_cols = lhs.num_cols();

  DenseVectorType& p = scratch[0];
  DenseVectorType& r = scratch[1];
  DenseVectorType& z = scratch[2];
  DenseVectorType& tmp = scratch[3];

  LinearSolver::Summary summary;
  summary.termination_type = LinearSolverTerminationType::NO_CONVERGENCE;
  summary.message = "Maximum number of iterations reached.";
  summary.num_iterations = 0;

  const double norm_b = rhs.norm();
  if (norm_b == 0.0) {
    solution.setZero();
    summary.termination_type = LinearSolverTerminationType::SUCCESS;
    summary.message = "Convergence. |b| = 0.";
    return summary;
  }

  const double tol_r = options.r_tolerance * norm_b;

  tmp.setZero();
  lhs.RightMultiply(solution, tmp);
  r = rhs - tmp;
  double norm_r = r.norm();
  if (options.min_num_iterations == 0 && norm_r <= tol_r) {
    summary.termination_type = LinearSolverTerminationType::SUCCESS;
    summary.message =
        StringPrintf("Convergence. |r| = %e <= %e.", norm_r, tol_r);
    return summary;
  }

  double rho = 1.0;

  // Initial value of the quadratic model Q = x'Ax - 2 * b'x.
  double Q0 = -1.0 * solution.dot(rhs + r);

  for (summary.num_iterations = 1;; ++summary.num_iterations) {
    z.setZero();
    preconditioner.RightMultiply(r, z);

    double last_rho = rho;
    rho = r.dot(z);
    if (IsZeroOrInfinity(rho)) {
      summary.termination_type = LinearSolverTerminationType::FAILURE;
      summary.message = StringPrintf("Numerical failure. rho = r'z = %e.", rho);
      break;
    }

    if (summary.num_iterations == 1) {
      p = z;
    } else {
      double beta = rho / last_rho;
      if (IsZeroOrInfinity(beta)) {
        summary.termination_type = LinearSolverTerminationType::FAILURE;
        summary.message = StringPrintf(
            "Numerical failure. beta = rho_n / rho_{n-1} = %e, "
            "rho_n = %e, rho_{n-1} = %e",
            beta,
            rho,
            last_rho);
        break;
      }
      p = z + beta * p;
    }

    DenseVectorType& q = z;
    q.setZero();
    lhs.RightMultiply(p, q);
    const double pq = p.dot(q);
    if ((pq <= 0) || std::isinf(pq)) {
      summary.termination_type = LinearSolverTerminationType::NO_CONVERGENCE;
      summary.message = StringPrintf(
          "Matrix is indefinite, no more progress can be made. "
          "p'q = %e. |p| = %e, |q| = %e",
          pq,
          p.norm(),
          q.norm());
      break;
    }

    const double alpha = rho / pq;
    if (std::isinf(alpha)) {
      summary.termination_type = LinearSolverTerminationType::FAILURE;
      summary.message = StringPrintf(
          "Numerical failure. alpha = rho / pq = %e, rho = %e, pq = %e.",
          alpha,
          rho,
          pq);
      break;
    }

    solution = solution + alpha * p;

    // Ideally we would just use the update r = r - alpha*q to keep
    // track of the residual vector. However this estimate tends to
    // drift over time due to round off errors. Thus every
    // residual_reset_period iterations, we calculate the residual as
    // r = b - Ax. We do not do this every iteration because this
    // requires an additional matrix vector multiply which would
    // double the complexity of the CG algorithm.
    if (summary.num_iterations % options.residual_reset_period == 0) {
      tmp.setZero();
      lhs.RightMultiply(solution, tmp);
      r = rhs - tmp;
    } else {
      r = r - alpha * q;
    }

    // Quadratic model based termination.
    //   Q1 = x'Ax - 2 * b' x.
    const double Q1 = -1.0 * solution.dot(rhs + r);

    // For PSD matrices A, let
    //
    //   Q(x) = x'Ax - 2b'x
    //
    // be the cost of the quadratic function defined by A and b. Then,
    // the solver terminates at iteration i if
    //
    //   i * (Q(x_i) - Q(x_i-1)) / Q(x_i) < q_tolerance.
    //
    // This termination criterion is more useful when using CG to
    // solve the Newton step. This particular convergence test comes
    // from Stephen Nash's work on truncated Newton
    // methods. References:
    //
    //   1. Stephen G. Nash & Ariela Sofer, Assessing A Search
    //   Direction Within A Truncated Newton Method, Operation
    //   Research Letters 9(1990) 219-221.
    //
    //   2. Stephen G. Nash, A Survey of Truncated Newton Methods,
    //   Journal of Computational and Applied Mathematics,
    //   124(1-2), 45-59, 2000.
    //
    const double zeta = summary.num_iterations * (Q1 - Q0) / Q1;
    if (zeta < options.q_tolerance &&
        summary.num_iterations >= options.min_num_iterations) {
      summary.termination_type = LinearSolverTerminationType::SUCCESS;
      summary.message =
          StringPrintf("Iteration: %d Convergence: zeta = %e < %e. |r| = %e",
                       summary.num_iterations,
                       zeta,
                       options.q_tolerance,
                       r.norm());
      break;
    }
    Q0 = Q1;

    // Residual based termination.
    norm_r = r.norm();
    if (norm_r <= tol_r &&
        summary.num_iterations >= options.min_num_iterations) {
      summary.termination_type = LinearSolverTerminationType::SUCCESS;
      summary.message =
          StringPrintf("Iteration: %d Convergence. |r| = %e <= %e.",
                       summary.num_iterations,
                       norm_r,
                       tol_r);
      break;
    }

    if (summary.num_iterations >= options.max_num_iterations) {
      break;
    }
  }

  return summary;
}

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_CONJUGATE_GRADIENTS_SOLVER_H_
