#include "threads/fixed-point.h"

/* From int to fixed. */
inline fixed
itof (int n){
	return n << FIXED_Q;
}

/* From fixed to int. */
inline int
ftoi (fixed x){
	return x >> FIXED_Q;
}

/* From fixed to percent. */
inline int
ftopc (fixed x){
  return ftoi_round(fmultn(x,100));
}

/* Round to zero. */
inline int
ftoi_round (fixed x){
	return (x>=0) ? (x + (FIXED_F>>1)) >> FIXED_Q 
			: (x - (FIXED_F>>1)) >> FIXED_Q;
}

/* Add two fixed numbers. */
inline fixed
fadd (fixed op1, fixed op2){
	return op1 + op2;
}

/* Subtract two fixed numbers. */
inline fixed
fsub (fixed op1, fixed op2){
	return op1 - op2;
}

/* Multiply two fixed numbers. */
inline fixed
fmult (fixed op1, fixed op2){
	return (((int64_t) op1) * op2) >> FIXED_Q;
}

/* Divide two fixed numbers. */
inline fixed
fdiv (fixed op1, fixed op2){
	return (((int64_t) op1) << FIXED_Q) / op2;
}

/* Add fixed number with n. */
inline fixed
faddn (fixed op1, int n){
	return op1 + (n << FIXED_Q);
}

/* Subtract fixed number with n. */
inline fixed
fsubn (fixed op1, int n){
	return op1 - (n << FIXED_Q);
}

/* Multiply fixed number with n. */
inline fixed
fmultn (fixed op1, int n){
	return op1 * n;
}

/* Divide fixed number with n. */
inline fixed
fdivn (fixed op1, int n){
	return op1 / n;
}
