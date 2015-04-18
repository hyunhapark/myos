#include "threads/fixed-point.h"

inline fixed
itof (int n){      // int to fixed
	return n << FIXED_Q;
}

inline int
ftoi (fixed x){    // fixed to int
	return x >> FIXED_Q;
}

inline int
ftopc (fixed x){    // fixed to percent
  return ftoi_round(fmultn(x,100));
}

inline int
ftoi_round (fixed x){
	return (x>=0) ? (x + (FIXED_F>>1)) >> FIXED_Q 
			: (x - (FIXED_F>>1)) >> FIXED_Q;
}

inline fixed
fadd (fixed op1, fixed op2){
	return op1 + op2;
}

inline fixed
fsub (fixed op1, fixed op2){
	return op1 - op2;
}

inline fixed
fmult (fixed op1, fixed op2){
	return (((int64_t) op1) * op2) >> FIXED_Q;
}

inline fixed
fdiv (fixed op1, fixed op2){
	return (((int64_t) op1) << FIXED_Q) / op2;
}

inline fixed
faddn (fixed op1, int n){
	return op1 + (n << FIXED_Q);
}

inline fixed
fsubn (fixed op1, int n){
	return op1 - (n << FIXED_Q);
}

inline fixed
fmultn (fixed op1, int n){
	return op1 * n;
}

inline fixed
fdivn (fixed op1, int n){
	return op1 / n;
}
