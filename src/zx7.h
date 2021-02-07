#ifndef _ZX7_H
#define _ZX7_H

#include <stdbool.h>

#define MAX_OFFSET  2176  /* range 1..2176 */
#define MAX_LEN    65536  /* range 2..65536 */

typedef struct optimal_t {
    size_t bits;
    int offset;
    int len;
} Optimal;

Optimal *zx7_optimize(unsigned char *input_data, size_t input_size, long skip);

void zx7_reverse(unsigned char *first, unsigned char *last);
unsigned char *zx7_compress(unsigned char *input_data, size_t input_size, bool backwards_mode, size_t *output_size);
void zx7_decompress(unsigned char *in_data, unsigned char *out_data);

#endif
