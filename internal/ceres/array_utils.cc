// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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

#include "ceres/array_utils.h"

#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ceres/fpclassify.h"
#include "ceres/stringprintf.h"
#include "ceres/map_util.h"

namespace ceres {
namespace internal {

// It is a near impossibility that user code generates this exact
// value in normal operation, thus we will use it to fill arrays
// before passing them to user code. If on return an element of the
// array still contains this value, we will assume that the user code
// did not write to that memory location.
const double kImpossibleValue = 1e302;

bool IsArrayValid(const int size, const double* x) {
  if (x != NULL) {
    for (int i = 0; i < size; ++i) {
      if (!IsFinite(x[i]) || (x[i] == kImpossibleValue))  {
        return false;
      }
    }
  }
  return true;
}

int FindInvalidValue(const int size, const double* x) {
  if (x == NULL) {
    return size;
  }

  for (int i = 0; i < size; ++i) {
    if (!IsFinite(x[i]) || (x[i] == kImpossibleValue))  {
      return i;
    }
  }

  return size;
};

void InvalidateArray(const int size, double* x) {
  if (x != NULL) {
    for (int i = 0; i < size; ++i) {
      x[i] = kImpossibleValue;
    }
  }
}

void AppendArrayToString(const int size, const double* x, string* result) {
  for (int i = 0; i < size; ++i) {
    if (x == NULL) {
      StringAppendF(result, "Not Computed  ");
    } else {
      if (x[i] == kImpossibleValue) {
        StringAppendF(result, "Uninitialized ");
      } else {
        StringAppendF(result, "%12g ", x[i]);
      }
    }
  }
}

void CompactifyArray(vector<int>* array_ptr) {
  vector<int>& array = *array_ptr;
  const set<int> unique_group_ids(array.begin(), array.end());
  map<int, int> group_id_map;
  for (set<int>::const_iterator it = unique_group_ids.begin();
       it != unique_group_ids.end();
       ++it) {
    InsertOrDie(&group_id_map, *it, group_id_map.size());
  }

  for (int i = 0; i < array.size(); ++i) {
    array[i] = group_id_map[array[i]];
  }
}

}  // namespace internal
}  // namespace ceres
