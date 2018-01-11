// Ceres Solver - A fast non-linear least squares minimizer
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
// Author: mierle@gmail.com (Keir Mierle)

#include "ceres/solver.h"

#include <limits>
#include <cmath>
#include <vector>
#include "gtest/gtest.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/sized_cost_function.h"
#include "ceres/problem.h"
#include "ceres/problem_impl.h"

namespace ceres {
namespace internal {

const double kUninitialized = 1e302;

// Generally multiple inheritance is a terrible idea, but in this (test)
// case it makes for a relatively elegant test implementation.
struct WigglyBowlCostFunctionAndEvaluationCallback :
      SizedCostFunction<2, 2>,
      EvaluationCallback  {

  explicit WigglyBowlCostFunctionAndEvaluationCallback(double *parameter)
      : EvaluationCallback(),
        parameter(parameter),
        prepare_num_calls(0),
        evaluate_num_calls(0),
        evaluate_last_parameter_value(kUninitialized) {}

  virtual ~WigglyBowlCostFunctionAndEvaluationCallback() {}

  // Evaluation callback interface. This checks that all the preconditions are
  // met at the point that Ceres calls into it.
  virtual void PrepareForEvaluation(bool evaluate_jacobians,
                                    bool new_evaluation_point) {
    // Check: Prepare() & Evaluate() come in pairs, in that order. Before this
    // call, the number of calls excluding this one should match.
    EXPECT_EQ(prepare_num_calls, evaluate_num_calls);

    // Check: new_evaluation_point indicates that the parameter has changed.
    if (new_evaluation_point) {
      // If it's a new evaluation point, then the parameter should have
      // changed. Technically, it's not required that it must change but
      // in practice it does, and that helps with testing.
      EXPECT_NE(evaluate_last_parameter_value, *parameter);
      EXPECT_NE(prepare_parameter_value, *parameter);
    } else {
      // If this is the same evaluation point as last time, ensure that
      // the parameters match both from the previous evaluate, the
      // previous prepare, and the current prepare.
      EXPECT_EQ(evaluate_last_parameter_value, prepare_parameter_value);
      EXPECT_EQ(evaluate_last_parameter_value, *parameter);
    }

    // Save details for to check at the next call to Evaluate().
    prepare_num_calls++;
    prepare_requested_jacobians = evaluate_jacobians;
    prepare_new_evaluation_point = new_evaluation_point;
    prepare_parameter_value = *parameter;
  }

  // Cost function interface. This checks that preconditions that were
  // set as part of the PrepareForEvaluation() call are met in this one.
  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const {
    // Cost function implementation of the "Wiggly Bowl" function:
    //
    //   1/2 * [(y - a*sin(x))^2 + x^2],
    //
    // expressed as a Ceres cost function with two residuals:
    //
    //   r[0] = y - a*sin(x)
    //   r[1] = x.
    //
    // This is harder to optimize than the Rosenbrock function because the
    // minimizer has to navigate a sine-shaped valley while descending the 1D
    // parabola formed along the y axis. Note that the "a" needs to be more
    // than 5 to get a strong enough wiggle effect in the cost surface to
    // trigger failed iterations in the optimizer.
    const double a = 10.0;
    double x = (*parameters)[0];
    double y = (*parameters)[1];  // y is ignored for checking preconditions.
    residuals[0] = y - a * sin(x);
    residuals[1] = x;
    if (jacobians != NULL) {
      (*jacobians)[2 * 0 + 0] = - a * cos(x);  // df1/dx
      (*jacobians)[2 * 0 + 1] = 1.0;           // df1/dy
      (*jacobians)[2 * 1 + 0] = 1.0;           // df2/dx
      (*jacobians)[2 * 1 + 1] = 0.0;           // df2/dy
    }

    // Check: PrepareForEvaluation() & Evaluate() come in pairs, in that order.
    EXPECT_EQ(prepare_num_calls, evaluate_num_calls + 1);

    // Check: if new_evaluation_point indicates that the parameter has
    // changed, it has changed; otherwise it is the same.
    if (prepare_new_evaluation_point) {
      EXPECT_NE(evaluate_last_parameter_value, x);
    } else {
      EXPECT_NE(evaluate_last_parameter_value, kUninitialized);
      EXPECT_EQ(evaluate_last_parameter_value, x);
    }

    // Check: Parameter matches value in in parameter blocks during prepare.
    EXPECT_EQ(prepare_parameter_value, x);

    // Check: jacobians are requested if they were in PrepareForEvaluation().
    EXPECT_EQ(prepare_requested_jacobians, jacobians != NULL);

    evaluate_num_calls++;
    evaluate_last_parameter_value = x;
    return true;
  }

  // Pointer to the parameter block associated with this cost function.
  // Contents should get set by Ceres before calls to PrepareForEvaluation()
  // and Evaluate().
  double* parameter;

  // Track state: PrepareForEvaluation().
  //
  // These track details from the PrepareForEvaluation() call (hence the
  // "prepare_" prefix), which are checked for consistency in Evaluate().
  int prepare_num_calls;
  bool prepare_requested_jacobians;
  bool prepare_new_evaluation_point;
  double prepare_parameter_value;

  // Track state: Evaluate().
  //
  // These track details from the Evaluate() call (hence the "evaluate_"
  // prefix), which are then checked for consistency in the calls to
  // PrepareForEvaluation(). Mutable is reasonable for this case.
  mutable int evaluate_num_calls;
  mutable double evaluate_last_parameter_value;
};

TEST(EvaluationCallback, WithTrustRegionMinimizer) {
  double parameters[2] = {50.0, 50.0};
  const double original_x = parameters[0];

  WigglyBowlCostFunctionAndEvaluationCallback cost_function(parameters);
  Problem::Options problem_options;
  problem_options.cost_function_ownership = DO_NOT_TAKE_OWNERSHIP;
  Problem problem(problem_options);
  problem.AddResidualBlock(&cost_function, NULL, parameters);

  Solver::Options options;
  options.linear_solver_type = DENSE_QR;
  options.max_num_iterations = 300;  // Cost function is hard.
  options.evaluation_callback = &cost_function;

  Solver::Summary summary;

  // Run the solve. Checking is done inside the cost function / callback.
  Solve(options, &problem, &summary);

  // Ensure that this was a hard cost function (not all steps succeed).
  EXPECT_GT(summary.num_successful_steps, 10);
  EXPECT_GT(summary.num_unsuccessful_steps, 10);

  // Ensure PrepareForEvaluation() is called the appropriate number of times.
  EXPECT_EQ(cost_function.prepare_num_calls,
            // Unsuccessful steps are evaluated only once (no jacobians).
            summary.num_unsuccessful_steps +
            // Successful steps are evaluated twice: with and without jacobians.
            2 * summary.num_successful_steps
            // Final iteration doesn't re-evaluate the jacobian.
            // Note: This may be sensitive to tweaks to the TR algorithm; if
            // this becomes too brittle, remove this EXPECT_EQ() entirely.
            - 1);

  // Ensure the callback calls ran a reasonable number of times.
  EXPECT_GT(cost_function.prepare_num_calls, 0);
  EXPECT_GT(cost_function.evaluate_num_calls, 0);
  EXPECT_EQ(cost_function.prepare_num_calls,
            cost_function.evaluate_num_calls);

  // Ensure that the parameters did actually change.
  EXPECT_NE(parameters[0], original_x);
}

void WithLineSearchMinimizerImpl(
    LineSearchType line_search,
    LineSearchDirectionType line_search_direction,
    LineSearchInterpolationType line_search_interpolation) {
  double parameters[2] = {50.0, 50.0};
  const double original_x = parameters[0];

  WigglyBowlCostFunctionAndEvaluationCallback cost_function(parameters);
  Problem::Options problem_options;
  problem_options.cost_function_ownership = DO_NOT_TAKE_OWNERSHIP;
  Problem problem(problem_options);
  problem.AddResidualBlock(&cost_function, NULL, parameters);

  Solver::Options options;
  options.linear_solver_type = DENSE_QR;
  options.max_num_iterations = 300;  // Cost function is hard.
  options.minimizer_type = ceres::LINE_SEARCH;
  options.evaluation_callback = &cost_function;
  options.line_search_type = line_search;
  options.line_search_direction_type = line_search_direction;
  options.line_search_interpolation_type = line_search_interpolation;

  Solver::Summary summary;

  // Run the solve. Checking is done inside the cost function / callback.
  Solve(options, &problem, &summary);

  EXPECT_GT(summary.num_line_search_steps, 10);
  EXPECT_GT(cost_function.prepare_num_calls, 30);
  EXPECT_EQ(cost_function.prepare_num_calls,
            cost_function.evaluate_num_calls);
  EXPECT_NE(parameters[0], original_x);
}

// Wolfe with L-BFGS.
TEST(EvaluationCallback, WithLineSearchMinimizerWolfeLbfgsCubic) {
  WithLineSearchMinimizerImpl(WOLFE, LBFGS, CUBIC);
}
TEST(EvaluationCallback, WithLineSearchMinimizerWolfeLbfgsBisection) {
  // XXX - fails; detects re-evaluating with same point when new_point = true.
  WithLineSearchMinimizerImpl(WOLFE, LBFGS, BISECTION);
}
TEST(EvaluationCallback, WithLineSearchMinimizerWolfeLbfgsQuadratic) {
  WithLineSearchMinimizerImpl(WOLFE, LBFGS, QUADRATIC);
}

// Wolfe with full BFGS.
TEST(EvaluationCallback, WithLineSearchMinimizerWolfeBfgsCubic) {
  WithLineSearchMinimizerImpl(WOLFE, BFGS, CUBIC);
}
TEST(EvaluationCallback, WithLineSearchMinimizerWolfeBfgsBisection) {
  // XXX - fails; detects re-evaluating with same point when new_point = true.
  WithLineSearchMinimizerImpl(WOLFE, BFGS, BISECTION);
}
TEST(EvaluationCallback, WithLineSearchMinimizerWolfeBfgsQuadratic) {
  WithLineSearchMinimizerImpl(WOLFE, BFGS, QUADRATIC);
}

// Armijo with nonlinear conjugate gradient.
TEST(EvaluationCallback, WithLineSearchMinimizerArmijoCubic) {
  WithLineSearchMinimizerImpl(ARMIJO, NONLINEAR_CONJUGATE_GRADIENT, CUBIC);
}

TEST(EvaluationCallback, WithLineSearchMinimizerArmijoBisection) {
  WithLineSearchMinimizerImpl(ARMIJO, NONLINEAR_CONJUGATE_GRADIENT, BISECTION);
}

TEST(EvaluationCallback, WithLineSearchMinimizerArmijoQuadratic) {
  WithLineSearchMinimizerImpl(ARMIJO, NONLINEAR_CONJUGATE_GRADIENT, QUADRATIC);
}

}  // namespace internal
}  // namespace ceres
