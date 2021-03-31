/*
 * (c) Copyright 2021 by Einar Saukas. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of its author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include "zx0.h"

unsigned char *input_data;
unsigned char *output_data;
size_t input_index;
size_t output_index;
size_t input_size;
size_t output_size;
int bit_mask;
int bit_value;
int backtrack;
int last_byte;
int last_offset;

BLOCK *ghost_root = NULL;
BLOCK *dead_array = NULL;
int dead_array_size = 0;

int bit_index;
int diff;

BLOCK *allocate(int bits, int index, int offset, int length, BLOCK *chain) {
    BLOCK *ptr;

    if (ghost_root) {
        ptr = ghost_root;
        ghost_root = ptr->ghost_chain;
        if (ptr->chain) {
            if (!--ptr->chain->references) {
                ptr->chain->ghost_chain = ghost_root;
                ghost_root = ptr->chain;
            }
        }
    } else {
        if (!dead_array_size) {
            dead_array = (BLOCK *)malloc(QTY_BLOCKS*sizeof(BLOCK));
            if (!dead_array) {
                fprintf(stderr, "Error: Insufficient memory\n");
                exit(1);
            }
            dead_array_size = QTY_BLOCKS;
        }
        ptr = &dead_array[--dead_array_size];
    }
    ptr->bits = bits;
    ptr->index = index;
    ptr->offset = offset;
    ptr->length = length;
    if (chain)
        chain->references++;
    ptr->chain = chain;
    ptr->references = 0;
    return ptr;
}

void assign(BLOCK **ptr, BLOCK *chain) {
    chain->references++;
    if (*ptr) {
        if (!--(*ptr)->references) {
            (*ptr)->ghost_chain = ghost_root;
            ghost_root = *ptr;
        }
    }
    *ptr = chain;
}

int offset_ceiling(int index, int offset_limit) {
    return index > offset_limit ? offset_limit : index < INITIAL_OFFSET ? INITIAL_OFFSET : index;
}

int elias_gamma_bits(int value) {
    int bits = 1;
    while (value > 1) {
        bits += 2;
        value >>= 1;
    }
    return bits;
}

BLOCK* optimize(unsigned char *input_data, size_t input_size, int skip, int offset_limit) {
    BLOCK **last_literal;
    BLOCK **last_match;
    BLOCK **optimal;
    int* match_length;
    int* best_length;
    int best_length_size;
    int bits;
    int index;
    int offset;
    int length;
    int bits2;
    int dots = 2;
    int max_offset = offset_ceiling(input_size-1, offset_limit);

    /* allocate all main data structures at once */
    last_literal = (BLOCK **)calloc(max_offset+1, sizeof(BLOCK *));
    last_match = (BLOCK **)calloc(max_offset+1, sizeof(BLOCK *));
    optimal = (BLOCK **)calloc(input_size+1, sizeof(BLOCK *));
    match_length = (int *)calloc(max_offset+1, sizeof(int));
    best_length = (int *)malloc((input_size+1)*sizeof(int));
    if (!last_literal || !last_match || !optimal || !match_length || !best_length) {
         fprintf(stderr, "Error: Insufficient memory\n");
         exit(1);
    }
    best_length[2] = 2;

    /* start with fake block */
    assign(&(last_match[INITIAL_OFFSET]), allocate(-1, skip-1, INITIAL_OFFSET, 0, NULL));

    printf("[");

    /* process remaining bytes */
    for (index = skip; index < input_size; index++) {
        best_length_size = 2;
        max_offset = offset_ceiling(index, offset_limit);
        for (offset = 1; offset <= max_offset; offset++) {
            if (index != skip && index >= offset && input_data[index] == input_data[index-offset]) {
                /* copy from last offset */
                if (last_literal[offset]) {
                    length = index-last_literal[offset]->index;
                    bits = last_literal[offset]->bits + 1 + elias_gamma_bits(length);
                    assign(&(last_match[offset]), allocate(bits, index, offset, length, last_literal[offset]));
                    if (!optimal[index] || optimal[index]->bits > bits)
                        assign(&(optimal[index]), last_match[offset]);
                }
                /* copy from new offset */
                if (++match_length[offset] > 1) {
                    if (best_length_size < match_length[offset]) {
                        bits = optimal[index-best_length[best_length_size]]->bits + elias_gamma_bits(best_length[best_length_size]-1);
                        do {
                            best_length_size++;
                            bits2 = optimal[index-best_length_size]->bits + elias_gamma_bits(best_length_size-1);
                            if (bits2 <= bits) {
                                best_length[best_length_size] = best_length_size;
                                bits = bits2;
                            } else {
                                best_length[best_length_size] = best_length[best_length_size-1];
                            }
                        } while(best_length_size < match_length[offset]);
                    }
                    length = best_length[match_length[offset]];
                    bits = optimal[index-length]->bits + 8 + elias_gamma_bits((offset-1)/128+1) + elias_gamma_bits(length-1);
                    if (!last_match[offset] || last_match[offset]->index != index || last_match[offset]->bits > bits) {
                        assign(&last_match[offset], allocate(bits, index, offset, length, optimal[index-length]));
                        if (!optimal[index] || optimal[index]->bits > bits)
                            assign(&(optimal[index]), last_match[offset]);
                    }
                }
            } else {
                /* copy literals */
                match_length[offset] = 0;
                if (last_match[offset]) {
                    length = index-last_match[offset]->index;
                    bits = last_match[offset]->bits + 1 + elias_gamma_bits(length) + length*8;
                    assign(&(last_literal[offset]), allocate(bits, index, 0, length, last_match[offset]));
                    if (!optimal[index] || optimal[index]->bits > bits)
                        assign(&(optimal[index]), last_literal[offset]);
                }
            }
        }

        if (index*MAX_SCALE/input_size > dots) {
            printf(".");
            fflush(stdout);
            dots++;
        }
    }

    printf("]\n");

    return optimal[input_size-1];
}

void reverse(unsigned char *first, unsigned char *last) {
    unsigned char c;

    while (first < last) {
        c = *first;
        *first++ = *last;
        *last-- = c;
    }
}

void read_bytes(int n, int *delta) {
    input_index += n;
    diff += n;
    if (diff > *delta)
        *delta = diff;
}

void write_byte(int value) {
    output_data[output_index++] = value;
    diff--;
}

void write_bytes(int offset, int length) {
    int i;

    if (offset > output_size+output_index) {
        fprintf(stderr, "Error: Invalid data\n");
        exit(1);
    }
    while (length-- > 0) {
        i = output_index-offset;
        write_byte(output_data[i >= 0 ? i : BUFFER_SIZE+i]);
    }
}

void write_bit(int value) {
    if (backtrack) {
        if (value)
            output_data[output_index-1] |= 1;
        backtrack = FALSE;
    } else {
        if (!bit_mask) {
            bit_mask = 128;
            bit_index = output_index;
            write_byte(0);
        }
        if (value)
            output_data[bit_index] |= bit_mask;
        bit_mask >>= 1;
    }
}

void write_interlaced_elias_gamma(int value, int backwards_mode) {
    int i;

    for (i = 2; i <= value; i <<= 1)
        ;
    i >>= 1;
    while ((i >>= 1) > 0) {
        write_bit(backwards_mode);
        write_bit(value & i);
    }
    write_bit(!backwards_mode);
}

int read_byte() {
    last_byte = input_data[input_index++];
    return last_byte;
}

int read_bit() {
    if (backtrack) {
        backtrack = FALSE;
        return last_byte & 1;
    }
    bit_mask >>= 1;
    if (bit_mask == 0) {
        bit_mask = 128;
        bit_value = read_byte();
    }
    return bit_value & bit_mask ? 1 : 0;
}

int read_interlaced_elias_gamma() {
    int value = 1;
    while (!read_bit()) {
        value = value << 1 | read_bit();
    }
    return value;
}

unsigned char *zx0_compress(unsigned char *input_data, size_t input_size, bool quick_mode, bool backwards_mode, size_t *out_size) {
    int skip = 0;
    
    if (backwards_mode)
        reverse(input_data, input_data+input_size-1);

    /* generate output file */
    BLOCK *optimal = optimize(input_data, input_size, 0, quick_mode ? MAX_OFFSET_ZX7 : MAX_OFFSET_ZX0);
    BLOCK *next;
    BLOCK *prev;
    int last_offset = INITIAL_OFFSET;
    int first = TRUE;
    int i;

    /* calculate and allocate output buffer */
    *out_size = (optimal->bits+18+7)/8;
    output_data = (unsigned char *)malloc(*out_size);
    if (!output_data) {
         fprintf(stderr, "Error: Insufficient memory\n");
         exit(1);
    }

    /* initialize delta */
    diff = *out_size-input_size+skip;
    int delta = 0;

    /* un-reverse optimal sequence */
    next = NULL;
    while (optimal) {
        prev = optimal->chain;
        optimal->chain = next;
        next = optimal;
        optimal = prev;
    }

    input_index = skip;
    output_index = 0;
    bit_mask = 0;

    for (optimal = next->chain; optimal; optimal = optimal->chain) {
        if (!optimal->offset) {
            /* copy literals indicator */
            if (first)
                first = FALSE;
            else
                write_bit(0);

            /* copy literals length */
            write_interlaced_elias_gamma(optimal->length, backwards_mode);

            /* copy literals values */
            for (i = 0; i < optimal->length; i++) {
                write_byte(input_data[input_index]);
                read_bytes(1, &delta);
            }
        } else if (optimal->offset == last_offset) {
            /* copy from last offset indicator */
            write_bit(0);

            /* copy from last offset length */
            write_interlaced_elias_gamma(optimal->length, backwards_mode);
            read_bytes(optimal->length, &delta);
        } else {
            /* copy from new offset indicator */
            write_bit(1);

            /* copy from new offset MSB */
            write_interlaced_elias_gamma((optimal->offset-1)/128+1, backwards_mode);

            /* copy from new offset LSB */
            if (backwards_mode)
                write_byte(((optimal->offset-1)%128)<<1);
            else
                write_byte((255-((optimal->offset-1)%128))<<1);
            backtrack = TRUE;

            /* copy from new offset length */
            write_interlaced_elias_gamma(optimal->length-1, backwards_mode);
            read_bytes(optimal->length, &delta);

            last_offset = optimal->offset;
        }
    }

    /* end marker */
    write_bit(1);
    write_interlaced_elias_gamma(256, backwards_mode);

    /* conditionally reverse output file */
    if (backwards_mode)
        reverse(output_data, output_data+*out_size-1);

    return output_data;
}

void zx0_decompress(unsigned char *in_data, unsigned char *out_data) {
    int length;
    int i;

    input_data = in_data;
    output_data = out_data;
    input_size = 0;
    input_index = 0;

    output_index = 0;
    output_size = 0;
    bit_mask = 0;
    backtrack = FALSE;
    last_offset = INITIAL_OFFSET;

COPY_LITERALS:
    length = read_interlaced_elias_gamma();
    for (i = 0; i < length; i++) {
        write_byte(read_byte());
    }
    if (read_bit()) {
        goto COPY_FROM_NEW_OFFSET;
    }

/*COPY_FROM_LAST_OFFSET:*/
    length = read_interlaced_elias_gamma();
    write_bytes(last_offset, length);
    if (!read_bit()) {
        goto COPY_LITERALS;
    }

COPY_FROM_NEW_OFFSET:
    last_offset = read_interlaced_elias_gamma();
    if (last_offset == 256) {
        return;
    }
    last_offset = ((last_offset-1)<<7)+128-(read_byte()>>1);
    backtrack = TRUE;
    length = read_interlaced_elias_gamma()+1;
    write_bytes(last_offset, length);
    if (read_bit()) {
        goto COPY_FROM_NEW_OFFSET;
    } else {
        goto COPY_LITERALS;
    }
}
