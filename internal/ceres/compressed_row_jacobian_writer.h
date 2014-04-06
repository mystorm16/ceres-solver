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
// Author: keir@google.com (Keir Mierle)
//
// A jacobian writer that directly writes to compressed row sparse matrices.

#ifndef CERES_INTERNAL_COMPRESSED_ROW_JACOBIAN_WRITER_H_
#define CERES_INTERNAL_COMPRESSED_ROW_JACOBIAN_WRITER_H_

#include "ceres/evaluator.h"
#include "ceres/scratch_evaluate_preparer.h"

namespace ceres {
namespace internal {

class CompressedRowSparseMatrix;
class Program;
class SparseMatrix;

class CompressedRowJacobianWriter {
 public:
  CompressedRowJacobianWriter(Evaluator::Options /* ignored */,
                              Program* program)
    : program_(program) {
  }

  static void PopulateJacobianBlocks(const Program* program,
                                     CompressedRowSparseMatrix* jacobian);
  static void GetOrderedParameterBlocks(
      const Program* program,
      int residual_id,
      vector<pair<int, int> >* evaluated_jacobian_blocks);

  // JacobianWriter interface.

  // Since the compressed row matrix has different layout than that assumed by
  // the cost functions, use scratch space to store the jacobians temporarily
  // then copy them over to the larger jacobian in the Write() function.
  ScratchEvaluatePreparer* CreateEvaluatePreparers(int num_threads) {
    return ScratchEvaluatePreparer::Create(*program_, num_threads);
  }

  SparseMatrix* CreateJacobian() const;

  void Write(int residual_id,
             int residual_offset,
             double **jacobians,
             SparseMatrix* base_jacobian);

 private:
  Program* program_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_COMPRESSED_ROW_JACOBIAN_WRITER_H_
