# Ceres Solver - A fast non-linear least squares minimizer
# Copyright 2018 Google Inc. All rights reserved.
# http://ceres-solver.org/
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
# Author: keir@google.com (Keir Mierle)
#
# Generate bundle adjustment tests as separate binaries. Since the bundle
# adjustment tests are fairly processing intensive, serializing them makes the
# tests take forever to run. Splitting them into separate binaries makes it
# easier to parallelize in continous integration systems, and makes local
# processing on multi-core workstations much faster.

# Product of ORDERINGS, THREAD_CONFIGS, and SOLVER_CONFIGS is the full set of
# tests to generate.
ORDERINGS = ["kAutomaticOrdering", "kUserOrdering"]
THREAD_CONFIGS = ["SolverConfig", "ThreadedSolverConfig"]

SOLVER_CONFIGS = [
  # Linear solver            Sparse backend      Preconditioner
  ('DENSE_SCHUR',            'NO_SPARSE',        'IDENTITY'),
  ('ITERATIVE_SCHUR',        'NO_SPARSE',        'JACOBI'),
  ('ITERATIVE_SCHUR',        'NO_SPARSE',        'SCHUR_JACOBI'),
  ('ITERATIVE_SCHUR',        'SUITE_SPARSE',     'CLUSTER_JACOBI'),
  ('ITERATIVE_SCHUR',        'SUITE_SPARSE',     'CLUSTER_TRIDIAGONAL'),
  ('SPARSE_NORMAL_CHOLESKY', 'SUITE_SPARSE',     'IDENTITY'),
  ('SPARSE_NORMAL_CHOLESKY', 'EIGEN_SPARSE',     'IDENTITY'),
  ('SPARSE_NORMAL_CHOLESKY', 'CX_SPARSE',        'IDENTITY'),
  ('SPARSE_SCHUR',           'SUITE_SPARSE',     'IDENTITY'),
  ('SPARSE_SCHUR',           'EIGEN_SPARSE',     'IDENTITY'),
  ('SPARSE_SCHUR',           'CX_SPARSE',        'IDENTITY'),
]


BUNDLE_ADJUSTMENT_TEST_TEMPLATE = (
"""// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
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
// ========================================
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
//=========================================
//
// This file is generated using generate_bundle_adjustment_tests.py.

#include "bundle_adjustment_test_util.h"
%(preprocessor_conditions_begin)s
namespace ceres {
namespace internal {

TEST_F(BundleAdjustmentTest,
       %(test_name)s) {  // NOLINT
  RunSolverForConfigAndExpectResidualsMatch(
      %(thread_config)s(
          %(linear_solver)s,
          %(sparse_backend)s,
          %(ordering)s,
          %(preconditioner)s));
}

}  // namespace internal
}  // namespace ceres
%(preprocessor_conditions_end)s
""")


def camelcasify(token):
  """Convert capitalized underscore tokens to camel case"""
  return ''.join([x.lower().capitalize() for x in token.split('_')])


def generate_bundle_test(linear_solver,
                         sparse_backend,
                         preconditioner,
                         ordering,
                         thread_config):
  """Generate a bundle adjustment test executable configured appropriately"""
  # Use a double underscore; otherwise the names are harder to understand.
  test_name = '_'.join((
      camelcasify(linear_solver),
      camelcasify(sparse_backend),
      camelcasify(preconditioner if preconditioner else 'NONE'),
      ordering[1:],  # Strip 'k'
      'Threads' if thread_config == 'ThreadedSolverConfig' else 'NoThreads'))

  # Initial template parameters (augmented more below).
  template_parameters = dict(
          linear_solver=linear_solver,
          sparse_backend=sparse_backend,
          preconditioner=preconditioner,
          ordering=ordering,
          thread_config=thread_config,
          test_name=test_name)

  # Accumulate appropriate #ifdef/#ifndefs for the solver's sparse backend.
  preprocessor_conditions_begin = []
  preprocessor_conditions_end = []
  if sparse_backend == 'SUITE_SPARSE':
    preprocessor_conditions_begin.append('#ifndef CERES_NO_SUITESPARSE')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_SUITESPARSE')
  elif sparse_backend == 'CX_SPARSE':
    preprocessor_conditions_begin.append('#ifndef CERES_NO_CXSPARSE')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_CXSPARSE')
  elif sparse_backend == 'EIGEN_SPARSE':
    preprocessor_conditions_begin.append('#ifdef CERES_USE_EIGEN_SPARSE')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_USE_EIGEN_SPARSE')

  # Accumulate appropriate #ifdef/#ifndefs for threading conditions.
  if thread_config == 'ThreadedSolverConfig':
    preprocessor_conditions_begin.append('#ifndef CERES_NO_THREADS')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_THREADS')

  # If there are #ifdefs, put newlines around them.
  if preprocessor_conditions_begin:
    preprocessor_conditions_begin.insert(0, '')
    preprocessor_conditions_begin.append('')
    preprocessor_conditions_end.insert(0, '')
    preprocessor_conditions_end.append('')

  template_parameters['preprocessor_conditions_begin'] = '\n'.join(
      preprocessor_conditions_begin)
  template_parameters['preprocessor_conditions_end'] = '\n'.join(
      preprocessor_conditions_end)

  filename = 'generated_bundle_adjustment_tests/ba_%s_test.cc' % test_name.lower()
  with open(filename, 'w') as fd:
    fd.write(BUNDLE_ADJUSTMENT_TEST_TEMPLATE % template_parameters)
  print 'Generated', test_name


if __name__ == '__main__':
  # Iterate over all the possible configurations, and generate the files.
  for linear_solver, sparse_backend, preconditioner in SOLVER_CONFIGS:
    for ordering in ORDERINGS:
      for thread_config in THREAD_CONFIGS:
        generate_bundle_test(linear_solver,
                             sparse_backend,
                             preconditioner,
                             ordering,
                             thread_config)
