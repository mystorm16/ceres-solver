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
// Author: strandmark@google.com (Petter Strandmark)
//
// Simple class for accessing PGM images.

#ifndef CERES_EXAMPLES_PGM_IMAGE_H_
#define CERES_EXAMPLES_PGM_IMAGE_H_

#include <string>
#include <vector>

namespace ceres {
namespace examples {

template<typename real>
class PGMImage {
 public:
  // Create and empty image
  PGMImage(unsigned width, unsigned height);
  // Load an image from file
  explicit PGMImage(std::string filename);
  // Sets an image to a constant
  void Set(double constant);

  // Reading dimensions
  unsigned width() const;
  unsigned height() const;
  unsigned NumPixels() const;

  // Get individual pixels
  real& Pixel(unsigned x, unsigned y);
  real  Pixel(unsigned x, unsigned y) const;
  real& Pixel(unsigned ind);
  real  Pixel(unsigned ind) const;
  unsigned LinearIndex(unsigned x, unsigned y) const;

  // Adds an image to another
  void operator+= (const PGMImage& image);
  // Adds a constant to an image
  void operator+= (real a);
  // Multiplies an image by a constant
  void operator*= (real a);

  // File access
  bool WriteToFile(std::string filename) const;
  bool ReadFromFile(std::string filename);

  // Accessing the image data directly
  bool SetData(const std::vector<real>& new_data);
  const std::vector<real>& data() const;

 protected:
  unsigned height_, width_;
  std::vector<real> data_;
};

}  // namespace examples
}  // namespace ceres


#endif  // CERES_EXAMPLES_PGM_IMAGE_H_
