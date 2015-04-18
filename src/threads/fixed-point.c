#include "threads/fixed-point.h"

fixed
itof (int n){      // int to fixed
	return n * FIXED_F;
}

int
ftoi (fixed x){    // fixed to int
	return x / FIXED_F;
}

int
ftopc (fixed x){    // fixed to percent
  return ftoi_round(fmultn(x,100));
}

int
ftoi_round (fixed x){
	return (x>=0) ? (x + (FIXED_F>>1)) / FIXED_F 
			: (x - (FIXED_F>>1)) / FIXED_F;
}

fixed
fadd (fixed op1, fixed op2){
	return op1 + op2;
}

fixed
fsub (fixed op1, fixed op2){
	return op1 - op2;
}

fixed
fmult (fixed op1, fixed op2){
	return ((int64_t) op1) * op2 / FIXED_F;
}

fixed
fdiv (fixed op1, fixed op2){
	return ((int64_t) op1) * FIXED_F / op2;
}

fixed
faddn (fixed op1, int n){
	return op1 + n * FIXED_F;
}

fixed
fsubn (fixed op1, int n){
	return op1 - n * FIXED_F;
}

fixed
fmultn (fixed op1, int n){
	return op1 * n;
}

fixed
fdivn (fixed op1, int n){
	return op1 / n;
}
