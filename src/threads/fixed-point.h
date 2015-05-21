#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdio.h>
#include <stdint.h>

/* Fixed point notation P.Q */
#define FIXED_P 17
#define FIXED_Q 14
#define FIXED_SP 18  /* Includes sign bit. */
#define FIXED_F (1<<FIXED_Q)

typedef int32_t fixed;

inline fixed itof (int n);
inline int ftoi (fixed x);
inline int ftopc (fixed x);
inline int ftoi_round (fixed x);
inline fixed fadd (fixed op1, fixed op2);
inline fixed fsub (fixed op1, fixed op2);
inline fixed fmult (fixed op1, fixed op2);
inline fixed fdiv (fixed op1, fixed op2);
inline fixed faddn (fixed op1, int n);
inline fixed fsubn (fixed op1, int n);
inline fixed fmultn (fixed op1, int n);
inline fixed fdivn (fixed op1, int n);


#endif	/* threads/fixed-point.h */
