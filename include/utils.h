#ifndef UTILS
#define UTILS_H 1

char *read_to_heap(const char *filename);
char *read_to_heap_bin(const char *filename, long *out_len);

#endif