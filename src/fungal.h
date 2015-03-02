
#ifndef H_FUNGAL
 #define H_FUNGAL

 #ifdef __cplusplus
  extern "C" {
 #endif

#include "asmtypes.h"

#define IS_VECTOR 01000000

const char *disasm(unsigned int inst);

unsigned int fungasm(const char *text);
unsigned int eval_scalar(const char *text, int *len);
 char *find_token(const char *text, int *len, unsigned int *value);
 unsigned int true_value(const char *equname, int len);

/* If fungasm() fails, it will return an out-of-range value,
 * and this indicator will point to a string describing a
 * problem with the input. */
extern const char *fungasm_error;

 #ifdef __cplusplus
  }
 #endif
#endif
