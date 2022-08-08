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

#ifndef CERES_INTERNAL_POWER_SERIES_EXPANSION_PRECONDITIONER_H_
#define CERES_INTERNAL_POWER_SERIES_EXPANSION_PRECONDITIONER_H_

#include "ceres/implicit_schur_complement.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/preconditioner.h"

// This is a preconditioner interface for power series expansion of Schur
// complement inverse as described in "Weber et al, Power Bundle Adjustment for
// Large-Scale 3D Reconstruction".

namespace ceres::internal {

class CERES_NO_EXPORT PowerSeriesExpansionPreconditioner
    : public Preconditioner {
 public:
  PowerSeriesExpansionPreconditioner(const ImplicitSchurComplement* s,
                                     Preconditioner::Options options);
  PowerSeriesExpansionPreconditioner(
      const PowerSeriesExpansionPreconditioner&) = delete;
  void operator=(const PowerSeriesExpansionPreconditioner&) = delete;
  ~PowerSeriesExpansionPreconditioner() override;

  void RightMultiply(const double* x, double* y) const final;
  bool Update(const LinearOperator& A, const double* D) final;
  int num_rows() const final;

 private:
  const ImplicitSchurComplement* s_;
  const Preconditioner::Options options_;
  mutable Vector b_init;
  mutable Vector b_temp;
  mutable Vector b_temp_previous;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_POWER_SERIES_EXPANSION_PRECONDITIONER_H_
