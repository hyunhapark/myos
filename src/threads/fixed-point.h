#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdio.h>
#include <stdint.h>

// fixed point notation P.Q
#define FIXED_P 17
#define FIXED_Q 14
#define FIXED_SP 18  // include sign bit
#define FIXED_F (1<<FIXED_Q)

typedef int32_t fixed;

inline fixed itof (int n);      // int to fixed
inline int ftoi (fixed x);    // fixed to int
inline int ftopc (fixed x);    // fixed to percent
inline int ftoi_round (fixed x);
inline fixed fadd (fixed op1, fixed op2);
inline fixed fsub (fixed op1, fixed op2);
inline fixed fmult (fixed op1, fixed op2);
inline fixed fdiv (fixed op1, fixed op2);
inline fixed faddn (fixed op1, int n);
inline fixed fsubn (fixed op1, int n);
inline fixed fmultn (fixed op1, int n);
inline fixed fdivn (fixed op1, int n);

/*
union _cvt1 {
	uint32_t f;
	struct wrapped1{unsigned u:FIXED_Q, o:FIXED_P, s:1;} w;
};

union _cvt2 {
	uint32_t f;
	struct wrapped2{unsigned u:FIXED_Q; int o:FIXED_SP;} w;
};
//*/

#endif	// threads/fixed-point.h
