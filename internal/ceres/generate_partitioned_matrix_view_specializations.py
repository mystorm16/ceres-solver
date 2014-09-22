# Ceres Solver - A fast non-linear least squares minimizer
# Copyright 2013 Google Inc. All rights reserved.
# http://code.google.com/p/ceres-solver/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Author: sameeragarwal@google.com (Sameer Agarwal)
#
# Script for explicitly generating template specialization of the
# PartitionedMatrixView class. Explicitly generating these
# instantiations in separate .cc files breaks the compilation into
# separate compilation unit rather than one large cc file.
#
# This script creates two sets of files.
#
# 1. partitioned_matrix_view_x_x_x.cc
# where the x indicates the template parameters and
#
# 2. partitioned_matrix_view.cc
#
# that contains a factory function for instantiating these classes
# based on runtime parameters.
#
# The list of tuples, specializations indicates the set of
# specializations that is generated.

# Set of template specializations to generate
SPECIALIZATIONS = [(2, 2, 2),
                   (2, 2, 3),
                   (2, 2, 4),
                   (2, 2, "Eigen::Dynamic"),
                   (2, 3, 3),
                   (2, 3, 4),
                   (2, 3, 9),
                   (2, 3, "Eigen::Dynamic"),
                   (2, 4, 3),
                   (2, 4, 4),
                   (2, 4, 8),
                   (2, 4, 9),
                   (2, 4, "Eigen::Dynamic"),
                   (2, "Eigen::Dynamic", "Eigen::Dynamic"),
                   (4, 4, 2),
                   (4, 4, 3),
                   (4, 4, 4),
                   (4, 4, "Eigen::Dynamic"),
                   ("Eigen::Dynamic", "Eigen::Dynamic", "Eigen::Dynamic")]
HEADER = """// Ceres Solver - A fast non-linear least squares minimizer
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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//
// Template specialization of PartitionedMatrixView.
//
// ========================================
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
//=========================================
//
// This file is generated using generate_partitioned_matrix_view_specializations.py.
// Editing it manually is not recommended.
"""

DYNAMIC_FILE = """

#include "ceres/partitioned_matrix_view_impl.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

template class PartitionedMatrixView<%s, %s, %s>;

}  // namespace internal
}  // namespace ceres
"""

SPECIALIZATION_FILE = """
// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_RESTRICT_SCHUR_SPECIALIZATION

#include "ceres/partitioned_matrix_view_impl.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

template class PartitionedMatrixView<%s, %s, %s>;

}  // namespace internal
}  // namespace ceres

#endif  // CERES_RESTRICT_SCHUR_SPECIALIZATION
"""

FACTORY_FILE_HEADER = """
#include "ceres/linear_solver.h"
#include "ceres/partitioned_matrix_view.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

PartitionedMatrixViewBase*
PartitionedMatrixViewBase::Create(const LinearSolver::Options& options,
                                  const BlockSparseMatrix& matrix) {
#ifndef CERES_RESTRICT_SCHUR_SPECIALIZATION
"""

FACTORY_CONDITIONAL = """  if ((options.row_block_size == %s) &&
      (options.e_block_size == %s) &&
      (options.f_block_size == %s)) {
    return new PartitionedMatrixView<%s, %s, %s>(
                 matrix, options.elimination_groups[0], options.num_threads);
  }
"""

FACTORY_FOOTER = """
#endif
  VLOG(1) << "Template specializations not found for <"
          << options.row_block_size << ","
          << options.e_block_size << ","
          << options.f_block_size << ">";
  return new PartitionedMatrixView<Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic>(
               matrix, options.elimination_groups[0]);
};

}  // namespace internal
}  // namespace ceres
"""


def SuffixForSize(size):
  if size == "Eigen::Dynamic":
    return "d"
  return str(size)


def SpecializationFilename(prefix, row_block_size, e_block_size, f_block_size):
  return "_".join([prefix] + map(SuffixForSize, (row_block_size,
                                                 e_block_size,
                                                 f_block_size)))


def Specialize():
  """
  Generate specialization code and the conditionals to instantiate it.
  """
  f = open("partitioned_matrix_view.cc", "w")
  f.write(HEADER)
  f.write(FACTORY_FILE_HEADER)

  for row_block_size, e_block_size, f_block_size in SPECIALIZATIONS:
    output = SpecializationFilename("generated/partitioned_matrix_view",
                                    row_block_size,
                                    e_block_size,
                                    f_block_size) + ".cc"
    fptr = open(output, "w")
    fptr.write(HEADER)

    template = SPECIALIZATION_FILE
    if (row_block_size == "Eigen::Dynamic" and
        e_block_size == "Eigen::Dynamic" and
        f_block_size == "Eigen::Dynamic"):
      template = DYNAMIC_FILE

    fptr.write(template % (row_block_size, e_block_size, f_block_size))
    fptr.close()

    f.write(FACTORY_CONDITIONAL % (row_block_size,
                                   e_block_size,
                                   f_block_size,
                                   row_block_size,
                                   e_block_size,
                                   f_block_size))
  f.write(FACTORY_FOOTER)
  f.close()


if __name__ == "__main__":
  Specialize()
