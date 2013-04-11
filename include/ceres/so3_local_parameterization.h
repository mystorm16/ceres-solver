// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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
// Author: sergey.vfx@gmail.com (Sergey Sharybin)
//         mierle@gmail.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_SO3_LOCAL_PARAMETERIZATION_H_
#define CERES_PUBLIC_SO3_LOCAL_PARAMETERIZATION_H_

#include "ceres/autodiff_local_parameterization.h"
#include "ceres/rotation.h"

namespace ceres {

// Plus functor which gets initial rotation R,
// rotation delta and computes
//
//   R_plus_delta = R + delta
//
// Where:
// - R is a 3x3 col-major rotation matrix
// - delta is an angle-axis rotation delta
// - R_plus_delta is a 3x3 col-major rotation
//   matrix which is a sum of R and delta
struct RotationMatrixPlus {
  template<typename T>
  bool operator()(const T* R,  // Rotation 3x3 col-major.
                  const T* delta,  // Angle-axis delta
                  T* R_plus_delta) const {
    T angle_axis[3];

    ceres::RotationMatrixToAngleAxis(R, angle_axis);

    angle_axis[0] += delta[0];
    angle_axis[1] += delta[1];
    angle_axis[2] += delta[2];

    ceres::AngleAxisToRotationMatrix(angle_axis, R_plus_delta);

    return true;
  }
};

// Local parameterization from 3x3 col-major rotation matrix
// space to angle-axis rotation space using auto differentiation
// for Jacobian computation.
//
// Usage example is when you need to optimize rotation-matrix
// with a solver, you simply pass it to residual block functor
// and set local parameterization for this parameter in the
// following way:
//
//   problem.SetParameterization(rotation_matrix_block
//                               new AutoDiffRotationMatrixParameterization);
//
// Note that you could share the same parameterization across
// different residual blocks or parameters
typedef AutoDiffLocalParameterization<RotationMatrixPlus, 9, 3>
      AutoDiffRotationMatrixParameterization;

}  // namespace ceres

#endif  // CERES_PUBLIC_SO3_LOCAL_PARAMETERIZATION_H_
