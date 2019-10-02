// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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
// Author: darius.rueckert@fau.de (Darius Rueckert)
//
//
// This file contains the basic expression type, which is used during code
// creation. Only assignment expressions of the following form are supported:
//
// result = [constant|binary_expr|functioncall]
//
// Examples:
// v_78 = v_28 / v_62;
// v_97 = exp(v_20);
// v_89 = 3.000000;
//
//
#ifndef CERES_PUBLIC_EXPRESSION_H_
#define CERES_PUBLIC_EXPRESSION_H_

#include <cmath>
#include <iosfwd>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace ceres {
namespace internal {

using ExpressionId = int;
static constexpr ExpressionId kInvalidExpressionId = -1;

// @brief A type-safe reference to 'Expression'.
//
// This class represents a scalar value that creates new expressions during
// evaluation. ExpressionRef can be used as template parameter for cost functors
// and Jets.
//
// ExpressionRef should be passed by value.
struct ExpressionRef {
  ExpressionRef() = default;

  // Create a constant expression directly from a double value.
  // v_0 = 123;
  ExpressionRef(double constant);

  // Returns v_id
  std::string ToString();

  // Compound operators (defined in expression_arithmetic.h)
  inline ExpressionRef& operator+=(ExpressionRef y);
  inline ExpressionRef& operator-=(ExpressionRef y);
  inline ExpressionRef& operator*=(ExpressionRef y);
  inline ExpressionRef& operator/=(ExpressionRef y);

  ExpressionId id = kInvalidExpressionId;
};

// @brief A reference to a comparison expressions.
//
// This additonal type is required, so that we can detect invalid conditions
// during compile time. For example, the following should create a compile time
// error:
//
// ExpressionRef a(5);
// CERES_IF(a){           // Error: Invalid conversion
// ...
//
// Aollowing will work:
//
// ExpressionRef a(5), b(7);
// ComparisonExpressionRef c = a < b;
// CERES_IF(c){
// ...
struct ComparisonExpressionRef {
  ExpressionRef id;
  explicit ComparisonExpressionRef(ExpressionRef id) : id(id) {}
};

enum class ExpressionType {
  // v_0 = 3.1415;
  COMPILE_TIME_CONSTANT,

  // For example a local member of the cost-functor.
  // v_0 = _observed_point_x;
  RUNTIME_CONSTANT,

  // Input parameter
  // v_0 = parameters[1][5];
  PARAMETER,

  // Output Variable Assignemnt
  // residual[0] = v_51;
  OUTPUT_ASSIGNMENT,

  // Trivial Assignment
  // v_1 = v_0;
  ASSIGNMENT,

  // Binary Arithmetic Operations
  // v_2 = v_0 + v_1
  PLUS,
  MINUS,
  MULTIPLICATION,
  DIVISION,

  // Unary Arithmetic Operation
  // v_1 = -(v_0);
  // v_2 = +(v_1);
  UNARY_MINUS,
  UNARY_PLUS,

  // Binary Comparision. (<,>,&&,...)
  // This is the only expressions which returns a 'bool'.
  // const bool v_2 = v_0 < v_1
  BINARY_COMPARE,

  // General Function Call.
  // v_5 = f(v_0,v_1,...)
  FUNCTION_CALL,

  // The ternary ?-operator. Separated from the general function call for easier
  // access.
  // v_3 = ternary(v_0,v_1,v_2);
  TERNARY,

  // No Operation. A placeholder for an 'empty' expressions which will be
  // optimized
  // out during code generation.
  NOP
};

// This class contains all data that is required to generate one line of code.
// Each line has the following form:
//
// lhs = rhs;
//
// The left hand side is the variable name given by its own id. The right hand
// side depends on the ExpressionType. For example, a COMPILE_TIME_CONSTANT
// expressions with id 4 generates the following line:
// v_4 = 3.1415;
class Expression {
 public:
  // These functions create the corresponding expression, add them to an
  // internal vector and return a reference to them.
  static ExpressionRef MakeConstant(double v);
  static ExpressionRef MakeRuntimeConstant(const std::string& name);
  static ExpressionRef MakeParameter(const std::string& name);
  static ExpressionRef MakeOutputAssignment(ExpressionRef v,
                                            const std::string& name);
  static ExpressionRef MakeAssignment(ExpressionRef v);
  static ExpressionRef MakeBinaryArithmetic(ExpressionType type_,
                                            ExpressionRef l,
                                            ExpressionRef r);
  static ExpressionRef MakeUnaryArithmetic(ExpressionRef v,
                                           ExpressionType type_);
  static ExpressionRef MakeBinaryCompare(const std::string& name,
                                         ExpressionRef l,
                                         ExpressionRef r);
  static ExpressionRef MakeFunctionCall(
      const std::string& name, const std::vector<ExpressionRef>& params_);
  static ExpressionRef MakeTernary(ComparisonExpressionRef c,
                                   ExpressionRef a,
                                   ExpressionRef b);

  // Returns true if the expression type is one of the basic math-operators:
  // +,-,*,/
  bool IsSimpleArithmetic() const;

  // If this expression is the compile time constant with the given value.
  // Used during optimization to collapse zero/one arithmetic operations.
  // b = a + 0;      ->    b = a;
  bool IsConstant(double constant) const;

  // Checks if "other" is identical to "this" so that one of the epxressions can
  // be replaced by a trivial assignemnt. Used during common subexpression
  // elimination.
  bool IsReplaceableBy(const Expression& other) const;

  // Replace this expression by 'other'.
  // The current id will be not replaced. That means other experssions
  // referencing this one stay valid.
  void Replace(const Expression& other);

  // if this expression has 'other' as a parameter
  bool DirectlyDependsOn(ExpressionRef other) const;

  // Converts this expression into a NOP
  void TurnIntoNop();

  // The return type as a string.
  // Usually "const double" except for comparison, which is "const bool".
  std::string ResultTypeAsString() const;

  // Returns the target name.
  //  v_0 = v_1 + v_2;
  // -> return "v_0"
  std::string LhsName() const;

 private:
  ExpressionRef id_;

  // Private constructor.
  // Only 'ExpressionPool' is allowed to create expressions.
  Expression(ExpressionType type, ExpressionRef id);
  friend class ExpressionTree;

  // Depending on the type this name is one of the following:
  //  (type == FUNCTION_CALL) -> the function name
  //  (type == PARAMETER)     -> the parameter name
  //  (type == OUTPUT_ASSIGN) -> the output variable name
  //  (type == BINARY_COMPARE)-> the comparison symbol "<","&&",...
  //  else                    -> unused
  std::string name_;

  ExpressionType type_ = ExpressionType::NOP;

  // Expressions have different number of parameters. For example a binary "+"
  // has 2 parameters and a function call so "sin" has 1 parameter. Here, a
  // reference to these paratmers is stored. Note: The order matters!
  std::vector<ExpressionRef> params_;

  // Only valid if type == COMPILE_TIME_CONSTANT
  double value_ = 0;
};

// The expression tree is stored linear in the data_ array. The order is
// identical to the execution order. Each expression can have multiple children
// and multiple parents.
// A is child of B     <=>  B has A as a parameter    <=> B.DirectlyDependsOn(A)
// A is parent of B    <=>  A has B as a parameter    <=> A.DirectlyDependsOn(B)
//
// Note:
// This is not a tree.
// It's an undirected, non-cyclic, unconnected graph.
class ExpressionTree {
 public:
  // Creates an expression and adds it to data_.
  // The returned reference will be invalid after this function is called again.
  Expression& MakeExpression(ExpressionType type);

  // Checks if A depends on B.
  // -> B is a descendant of A
  bool DependsOn(ExpressionRef A, ExpressionRef B) const;

  Expression& get(ExpressionRef id) { return data_[id.id]; }
  const Expression& get(ExpressionRef id) const { return data_[id.id]; }

 private:
  std::vector<Expression> data_;
};

// After calling this method, all operations on 'ExpressionRef' objects will be
// recorded into an internal array. You can obtain this array by calling
// StopRecordingExpressions.
//
// Performing expression operations before calling StartRecordingExpressions is
// an error.
void StartRecordingExpressions();

// Stops recording and returns all expressions that have been executed since the
// call to StartRecordingExpressions.
ExpressionTree StopRecordingExpressions();

}  // namespace internal
}  // namespace ceres
#endif
