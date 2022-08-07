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
// Author: keir@google.com (Keir Mierle)

#include "ceres/cgnr_solver.h"

#include <memory>
#include <utility>

#include "ceres/block_jacobi_preconditioner.h"
#include "ceres/cgnr_linear_operator.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/subset_preconditioner.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

#ifndef CERES_NO_CUDA
#include "ceres/cuda_cgnr_linear_operator.h"
#include "ceres/cuda_conjugate_gradients_solver.h"
#include "ceres/cuda_incomplete_cholesky_preconditioner.h"
#include "ceres/cuda_linear_operator.h"
#include "ceres/cuda_sparse_matrix.h"
#include "ceres/cuda_vector.h"
#endif  // CERES_NO_CUDA

namespace ceres::internal {

CgnrSolver::CgnrSolver(LinearSolver::Options options)
    : options_(std::move(options)) {
  if (options_.preconditioner_type != JACOBI &&
      options_.preconditioner_type != IDENTITY &&
      options_.preconditioner_type != SUBSET) {
    LOG(FATAL)
        << "Preconditioner = "
        << PreconditionerTypeToString(options_.preconditioner_type) << ". "
        << "Congratulations, you found a bug in Ceres. Please report it.";
  }
}

CgnrSolver::~CgnrSolver() = default;

LinearSolver::Summary CgnrSolver::SolveImpl(
    BlockSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("CgnrSolver::Solve");

  // Form z = Atb.
  Vector z(A->num_cols());
  z.setZero();
  A->LeftMultiply(b, z.data());
  if (!preconditioner_) {
    if (options_.preconditioner_type == JACOBI) {
      preconditioner_ = std::make_unique<BlockJacobiPreconditioner>(*A);
    } else if (options_.preconditioner_type == SUBSET) {
      Preconditioner::Options preconditioner_options;
      preconditioner_options.type = SUBSET;
      preconditioner_options.subset_preconditioner_start_row_block =
          options_.subset_preconditioner_start_row_block;
      preconditioner_options.sparse_linear_algebra_library_type =
          options_.sparse_linear_algebra_library_type;
      preconditioner_options.ordering_type = options_.ordering_type;
      preconditioner_options.num_threads = options_.num_threads;
      preconditioner_options.context = options_.context;
      preconditioner_ =
          std::make_unique<SubsetPreconditioner>(preconditioner_options, *A);
    }
  }

  if (preconditioner_) {
    preconditioner_->Update(*A, per_solve_options.D);
  }

  LinearSolver::PerSolveOptions cg_per_solve_options = per_solve_options;
  cg_per_solve_options.preconditioner = preconditioner_.get();

  // Solve (AtA + DtD)x = z (= Atb).
  VectorRef(x, A->num_cols()).setZero();
  CgnrLinearOperator lhs(*A, per_solve_options.D);
  event_logger.AddEvent("Setup");

  ConjugateGradientsSolver conjugate_gradient_solver(options_);
  LinearSolver::Summary summary =
      conjugate_gradient_solver.Solve(&lhs, z.data(), cg_per_solve_options, x);
  event_logger.AddEvent("Solve");
  return summary;
}

#ifndef CERES_NO_CUDA

CudaCgnrSolver::CudaCgnrSolver() = default;

CudaCgnrSolver::~CudaCgnrSolver() = default;

bool CudaCgnrSolver::Init(
    const LinearSolver::Options& options, std::string* error) {
  options_ = options;
  solver_ = CudaConjugateGradientsSolver::Create(options);
  if (solver_ == nullptr) {
    *error = "CudaConjugateGradientsSolver::Create failed.";
    return false;

  }
  if (!solver_->Init(options.context, error)) {
    return false;
  }
  return true;
}

std::unique_ptr<CudaCgnrSolver> CudaCgnrSolver::Create(
      LinearSolver::Options options, std::string* error) {
  CHECK(error != nullptr);
  if (options.preconditioner_type != IDENTITY) {
    *error = "CudaCgnrSolver does not support preconditioner type " +
        std::string(PreconditionerTypeToString(options.preconditioner_type)) + ". ";
    return nullptr;
  }
  std::unique_ptr<CudaCgnrSolver> solver(new CudaCgnrSolver());
  if (!solver->Init(options, error)) {
    return nullptr;
  }
  solver->options_ = options;
  return solver;
}

LinearSolver::Summary CudaCgnrSolver::SolveImpl(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  static const bool kDebug = false;
  EventLogger event_logger("CudaCgnrSolver::Solve");
  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;

  CudaSparseMatrix cuda_A;
  CudaVector cuda_b;
  CudaVector cuda_z;
  CudaVector cuda_x;
  CudaVector cuda_D;
  if (!cuda_A.Init(options_.context, &summary.message) ||
      !cuda_b.Init(options_.context, &summary.message) ||
      !cuda_z.Init(options_.context, &summary.message) ||
      !cuda_x.Init(options_.context, &summary.message) ||
      !cuda_D.Init(options_.context, &summary.message)) {
    return summary;
  }
  event_logger.AddEvent("Initialize");

  cuda_A.CopyFrom(*A);
  event_logger.AddEvent("A CPU to GPU Transfer");
  cuda_b.CopyFrom(b, A->num_rows());
  cuda_z.resize(A->num_cols());
  cuda_x.resize(A->num_cols());
  cuda_D.CopyFrom(per_solve_options.D, A->num_cols());
  event_logger.AddEvent("b CPU to GPU Transfer");

  std::unique_ptr<CudaPreconditioner> preconditioner(nullptr);
  if (options_.preconditioner_type == INCOMPLETE_CHOLESKY) {
    preconditioner = std::make_unique<CudaIncompleteCholeskyPreconditioner>();
    std::string message;
    CHECK(preconditioner->Init(options_.context, &message));
    CHECK(preconditioner->Update(cuda_A, cuda_D));
  }

  event_logger.AddEvent("Preconditioner Update");
  // Form z = Atb.
  cuda_z.setZero();
  cuda_A.LeftMultiply(cuda_b, &cuda_z);
  if (kDebug) printf("z = Atb\n");

  LinearSolver::PerSolveOptions cg_per_solve_options = per_solve_options;
  cg_per_solve_options.preconditioner = nullptr;

  // Solve (AtA + DtD)x = z (= Atb).
  cuda_x.setZero();
  if (!lhs_.Init(&cuda_A, &cuda_D, options_.context, &summary.message)) {
    summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;
    return summary;
  }

  event_logger.AddEvent("Setup");
  if (kDebug) printf("Solve (AtA + DtD)x = z (= Atb)\n");

  summary = solver_->Solve(
      &lhs_, preconditioner.get(), cuda_z, cg_per_solve_options, &cuda_x);
  cuda_x.CopyTo(x);
  event_logger.AddEvent("Solve");
  return summary;
}

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal
