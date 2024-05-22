#ifndef SWAP_H
#define SWAP_H

#include "stdio.h"
#include <stdlib.h> 
#include <string.h>

void int_swap(int* a, int* b);
void float_swap(float* a, float* b);
void swap(void* a, void* b, size_t length);

#endif
