#include "swap.h"

void int_swap(int* a, int* b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void float_swap(float* a, float* b) {
    float tmp = *a;
    *a = *b;
    *b = tmp;
}

void swap(void* a, void* b, size_t length) {
    void *temp = malloc(length);
    memcpy(temp, b, length);
    memcpy(b, a, length);
    memcpy(a, temp, length);
    free(temp);
}