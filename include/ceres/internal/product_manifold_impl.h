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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//         sergiu.deitsch@gmail.com (Sergiu Deitsch)
//

#ifndef CERES_PUBLIC_INTERNAL_PRODUCT_MANIFOLD_IMPL_H_
#define CERES_PUBLIC_INTERNAL_PRODUCT_MANIFOLD_IMPL_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <tuple>
#include <utility>

#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"

namespace ceres {

namespace internal {

template <typename T, std::size_t N>
inline std::array<T, N> ComputeExclusiveScan(const std::array<T, N>& values) {
  std::array<T, N> result;
  T init = 0;

  // TODO Replace by std::exclusive_scan once C++17 is available
  for (std::size_t i = 0; i != N; ++i) {
    result[i] = init;
    init += values[i];
  }

  return result;
}

template <typename... Manifolds>
class ProductManifoldImpl final : public Manifold {
 public:
  template <typename... Args>
  explicit ProductManifoldImpl(Args&&... manifolds)
      : buffer_size_{(std::max)(
            {(manifolds.TangentSize() * manifolds.AmbientSize())...})},
        ambient_sizes_{manifolds.AmbientSize()...},
        tangent_sizes_{manifolds.TangentSize()...},
        ambient_offsets_{ComputeExclusiveScan(ambient_sizes_)},
        tangent_offsets_{ComputeExclusiveScan(tangent_sizes_)},
        ambient_size_{
            std::accumulate(ambient_sizes_.begin(), ambient_sizes_.end(), 0)},
        tangent_size_{
            std::accumulate(tangent_sizes_.begin(), tangent_sizes_.end(), 0)},
        manifolds_{std::forward<Args>(manifolds)...} {}

  int AmbientSize() const override { return ambient_size_; }
  int TangentSize() const override { return tangent_size_; }

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override {
    return PlusImpl(
        x, delta, x_plus_delta, std::make_index_sequence<kNumManifolds>{});
  }

  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override {
    return MinusImpl(
        y, x, y_minus_x, std::make_index_sequence<kNumManifolds>{});
  }

  bool PlusJacobian(const double* x, double* jacobian_ptr) const override {
    MatrixRef jacobian(jacobian_ptr, AmbientSize(), TangentSize());
    jacobian.setZero();
    FixedArray<double> buffer(buffer_size_);

    return PlusJacobianImpl(
        x, jacobian, buffer, std::make_index_sequence<kNumManifolds>{});
  }

  bool MinusJacobian(const double* x, double* jacobian_ptr) const override {
    MatrixRef jacobian(jacobian_ptr, TangentSize(), AmbientSize());
    jacobian.setZero();
    FixedArray<double> buffer(buffer_size_);

    return MinusJacobianImpl(
        x, jacobian, buffer, std::make_index_sequence<kNumManifolds>{});
  }

 private:
  static constexpr std::size_t kNumManifolds = sizeof...(Manifolds);

  template <std::size_t Index0, std::size_t... Indices>
  bool PlusImpl(const double* x,
                const double* delta,
                double* x_plus_delta,
                std::index_sequence<Index0, Indices...>) const {
    if (!std::get<Index0>(manifolds_)
             .Plus(x + ambient_offsets_[Index0],
                   delta + tangent_offsets_[Index0],
                   x_plus_delta + ambient_offsets_[Index0])) {
      return false;
    }

    return PlusImpl(x, delta, x_plus_delta, std::index_sequence<Indices...>{});
  }

  static constexpr bool PlusImpl(const double* /*x*/,
                                 const double* /*delta*/,
                                 double* /*x_plus_delta*/,
                                 std::index_sequence<>) noexcept {
    return true;
  }

  template <std::size_t Index0, std::size_t... Indices>
  bool MinusImpl(const double* y,
                 const double* x,
                 double* y_minus_x,
                 std::index_sequence<Index0, Indices...>) const {
    if (!std::get<Index0>(manifolds_)
             .Minus(y + ambient_offsets_[Index0],
                    x + ambient_offsets_[Index0],
                    y_minus_x + tangent_offsets_[Index0])) {
      return false;
    }

    return MinusImpl(y, x, y_minus_x, std::index_sequence<Indices...>{});
  }

  static constexpr bool MinusImpl(const double* /*y*/,
                                  const double* /*x*/,
                                  double* /*y_minus_x*/,
                                  std::index_sequence<>) noexcept {
    return true;
  }

  template <std::size_t Index0, std::size_t... Indices>
  bool PlusJacobianImpl(const double* x,
                        MatrixRef& jacobian,
                        FixedArray<double>& buffer,
                        std::index_sequence<Index0, Indices...>) const {
    if (!std::get<Index0>(manifolds_)
             .PlusJacobian(x + ambient_offsets_[Index0], buffer.data())) {
      return false;
    }

    jacobian.block(ambient_offsets_[Index0],
                   tangent_offsets_[Index0],
                   ambient_sizes_[Index0],
                   tangent_sizes_[Index0]) =
        MatrixRef(
            buffer.data(), ambient_sizes_[Index0], tangent_sizes_[Index0]);

    return PlusJacobianImpl(
        x, jacobian, buffer, std::index_sequence<Indices...>{});
  }

  static constexpr bool PlusJacobianImpl(const double* /*x*/,
                                         MatrixRef& /*jacobian*/,
                                         FixedArray<double>& /*buffer*/,
                                         std::index_sequence<>) noexcept {
    return true;
  }

  template <std::size_t Index0, std::size_t... Indices>
  bool MinusJacobianImpl(const double* x,
                         MatrixRef& jacobian,
                         FixedArray<double>& buffer,
                         std::index_sequence<Index0, Indices...>) const {
    if (!std::get<Index0>(manifolds_)
             .MinusJacobian(x + ambient_offsets_[Index0], buffer.data())) {
      return false;
    }

    jacobian.block(tangent_offsets_[Index0],
                   ambient_offsets_[Index0],
                   tangent_sizes_[Index0],
                   ambient_sizes_[Index0]) =
        MatrixRef(
            buffer.data(), tangent_sizes_[Index0], ambient_sizes_[Index0]);

    return MinusJacobianImpl(
        x, jacobian, buffer, std::index_sequence<Indices...>{});
  }

  static constexpr bool MinusJacobianImpl(const double* /*x*/,
                                          MatrixRef& /*jacobian*/,
                                          FixedArray<double>& /*buffer*/,
                                          std::index_sequence<>) noexcept {
    return true;
  }

  int buffer_size_;
  std::array<int, kNumManifolds> ambient_sizes_;
  std::array<int, kNumManifolds> tangent_sizes_;
  std::array<int, kNumManifolds> ambient_offsets_;
  std::array<int, kNumManifolds> tangent_offsets_;
  int ambient_size_;
  int tangent_size_;
  std::tuple<Manifolds...> manifolds_;
};

}  // namespace internal

template <typename... Manifolds>
inline void ProductManifold::Initialize(Manifolds&&... manifolds) {
  impl_ = std::make_unique<internal::ProductManifoldImpl<Manifolds...>>(
      std::forward<Manifolds>(manifolds)...);
}

}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_PRODUCT_MANIFOLD_IMPL_H_
