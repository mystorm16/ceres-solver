// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
// Author: yangfan34@lenovo.com (Lenovo Research Shanghai)
//
// These codes aim to arm64-v8a platform, speeding up 64-bit float small
// matrices multiplication which can not utilize Eigen math library
// because of dynamic parameters.

.text
.align 4
.global MTM_mat1x4_p1_asm
//////////////////////////////////////////////////////
// void MTM_mat1x4_p1_asm(int ROW_A,
//                       const double* A, int lda,
//                       const double* B, int ldb,
//                       double* C);
//
// x0: ROW_A & the stride of Matrix A
// x1: A &  the pointer to Matrix A
// x2: lda & the stride of Matrix A
// x3: B &  the pointer to Matrix B
// x4: ldb & the stride of Matrix B
// x5: C &  the pointer to Matrix C
//
// p1: plus 1,  means C[index]+= tmp;
// m1: minus 1, means C[index]-= tmp;
// 00: zero,    means C[index] = tmp;
//
//                        Matrix A
//
//                        A0
//                        A1
//                        A2
//                        A3
//                        .
//                        .
//                        .
//
//  Matrix C              Matrix A'                  Matrix B
//
//  C0, C1, C2, C3   +=   A0, A1, A2, A3, ...    *   B0, B1, B2, B3
//                                                   B4, B5, B6, B7
//                                                   B8, B9, Ba, Bb
//                                                   Bc, Bd, Be, Bf
//                                                   . , . , . , .
//                                                   . , . , . , .
//                                                   . , . , . , .
/////////////////////////////////////////////////////

// Refer to chapter 5.1.1 from
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
//
// x9...x15: Temporary registers
// x8:       Indirect result location register
// x0...x7:  Parameter/result registers
//
// We can use these registers freely without saving/restoring their values

ROW_A .req x0
A     .req x1
lda   .req x2
B     .req x3
ldb   .req x4
C     .req x5

pa    .req x6
pb    .req x7
ai    .req x8
bi    .req x9
col_r .req x10
col_m .req x11
kk    .req x12

xx    .req x0 // reuse of x0

C0    .req d0
C1    .req d1
C2    .req d2
C3    .req d3

A0    .req d4
A1    .req d5
A2    .req d6
A3    .req d7

// Registers v8-v15 must be preserved by a callee across subroutine calls
// Refer to chapter 5.1.2 from
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
// So we avoid to use v8-v15

M0    .req d16
M1    .req d17
M2    .req d18
M3    .req d19
M4    .req d20
M5    .req d21
M6    .req d22
M7    .req d23
M8    .req d24
M9    .req d25
Ma    .req d26
Mb    .req d27
Mc    .req d28
Md    .req d29
Me    .req d30
Mf    .req d31

MTM_mat1x4_p1_asm:

	// load C0-C3 from Matrix C
	ldp   C0, C1, [C]
	ldp   C2, C3, [C, #16]

	and   col_r, ROW_A, #3
	sub   col_m, ROW_A, col_r
	mov   ai, #0
	mov   bi, #0
	mov   kk, #0

loop_4:
	cmp   kk, col_m
	b.GE  loop_4_end                   // finish the loop if kk >= col_m

	//////////////////////////////////////////////////////////////
	//  Matrix C              Matrix A'             Matrix B
	//
	//  C0, C1, C2, C3   +=   A0, A1, A2, A3    x   M0, M1, M2, M3
	//                                              M4, M5, M6, M7
	//                                              M8, M9, Ma, Mb
	//                                              Mc, Md, Me, Mf
	//
	//////////////////////////////////////////////////////////////

	// load A0-A3
	add   pa, A , ai, LSL #3           // pa = A + ai
	ldr   A0, [pa]
	ldr   A1, [pa,lda, LSL #3]
	add   ai, ai, lda, LSL #1          // ai += lda * 2
	add   pa, A , ai, LSL #3           // pa = A + ai
	ldr   A2, [pa]
	ldr   A3, [pa,lda, LSL #3]
	add   ai, ai, lda, LSL #1          // ai += lda * 2

	// load M0-Mf
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   M0, M1, [pb]
	ldp   M2, M3, [pb, #16]
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   M4, M5, [pb]
	ldp   M6, M7, [pb, #16]
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   M8, M9, [pb]
	ldp   Ma, Mb, [pb, #16]
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   Mc, Md, [pb]
	ldp   Me, Mf, [pb, #16]

	fmadd C0, A0, M0, C0
	fmadd C1, A0, M1, C1
	fmadd C2, A0, M2, C2
	fmadd C3, A0, M3, C3

	fmadd C0, A1, M4, C0
	fmadd C1, A1, M5, C1
	fmadd C2, A1, M6, C2
	fmadd C3, A1, M7, C3

	fmadd C0, A2, M8, C0
	fmadd C1, A2, M9, C1
	fmadd C2, A2, Ma, C2
	fmadd C3, A2, Mb, C3

	add   kk, kk, #4                   // kk += 4

	fmadd C0, A3, Mc, C0
	fmadd C1, A3, Md, C1
	fmadd C2, A3, Me, C2
	fmadd C3, A3, Mf, C3

	b     loop_4

loop_4_end:
	and   xx, col_r, #2
	cmp   xx, #2
	b.NE  mod2_end

	//////////////////////////////////////////////////////////////
	//  Matrix C              Matrix A'             Matrix B
	//
	//  C0, C1, C2, C3   +=   A0, A1            x   M0, M1, M2, M3
	//                                              M4, M5, M6, M7
	//
	//////////////////////////////////////////////////////////////

	// load A0, A1
	add   pa, A , ai, LSL #3           // pa = A + ai
	ldr   A0, [pa]
	ldr   A1, [pa, lda,LSL #3]
	add   ai, ai, lda, LSL #1          // ai += lda * 2

	// load M0-M7
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   M0, M1, [pb]
	ldp   M2, M3, [pb, #16]
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   M4, M5, [pb]
	ldp   M6, M7, [pb, #16]

	fmadd C0, A0, M0, C0
	fmadd C1, A0, M1, C1
	fmadd C2, A0, M2, C2
	fmadd C3, A0, M3, C3

	fmadd C0, A1, M4, C0
	fmadd C1, A1, M5, C1
	fmadd C2, A1, M6, C2
	fmadd C3, A1, M7, C3

mod2_end:
	and   xx, col_r, #1
	cmp   xx, #1
	b.NE  mod1_end

	//////////////////////////////////////////////////////////////
	//  Matrix C              Matrix A'             Matrix B
	//
	//  C0, C1, C2, C3   +=   A0                x   M0, M1, M2, M3
	//
	//////////////////////////////////////////////////////////////

	// load A0
	add   pa, A , ai, LSL #3           // pa = A + ai
	ldr   A0, [pa]

	// load M0-M3
	add   pb, B , bi, LSL #3           // pb = B + bi
	add   bi, bi, ldb                  // bi += ldb
	ldp   M0, M1, [pb]
	ldp   M2, M3, [pb, #16]

	fmadd C0, A0, M0, C0
	fmadd C1, A0, M1, C1
	fmadd C2, A0, M2, C2
	fmadd C3, A0, M3, C3

mod1_end:

	stp   C0, C1, [C]                  // write back to Matrix C
	stp   C2, C3, [C, #16]

	ret
