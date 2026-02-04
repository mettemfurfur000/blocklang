#ifndef UTILS
#define UTILS_H 1

#include <stdio.h>
#include <stdint.h>

char *read_to_heap(const char *filename);
char *read_to_heap_bin(const char *filename, long *out_len);

// Endianness detection and conversion

typedef enum {
    ENDIAN_NONE = 0,
    ENDIAN_UNKNOWN,
    ENDIAN_LITTLE,
    ENDIAN_BIG,
} endian_type;

endian_type get_system_endianness(void);

// Endianless read/write (for cross-platform compatibility)
// Only swaps bytes if needed based on system endianness

void fwrite_endianless(const void *ptr, size_t size, size_t count, FILE *stream);
void fread_endianless(void *ptr, size_t size, size_t count, FILE *stream);

#define WRITE(object, stream) fwrite_endianless(&(object), sizeof(object), 1, stream)
#define READ(object, stream) fread_endianless(&(object), sizeof(object), 1, stream)

#define WRITE_ARRAY(object, count, stream) fwrite_endianless((object), sizeof(*(object)), (count), stream)
#define READ_ARRAY(object, count, stream) fread_endianless((object), sizeof(*(object)), (count), stream)

#endif