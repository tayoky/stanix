#ifndef KERNEL_UNICODE_H
#define KERNEL_UNICODE_H

#include <stdint.h>
#include <sys/types.h>

int utf8_decode_char(const uint8_t *data, size_t count, int *codepoint);
ssize_t utf8_decode_buf(const uint8_t *data, size_t count, int *codepoints);
int utf8_encode_char(int codepoint, uint8_t *data);
ssize_t utf8_encode_buf(const int *codepoints, size_t count, uint8_t *data);

int utf16_decode_char(const uint16_t *data, size_t count, int *codepoint);
ssize_t utf16_decode_buf(const uint16_t *data, size_t count, int *codepoints);
int utf16_encode_char(int codepoint, uint16_t *data);
ssize_t utf16_encode_buf(const int *codepoints, size_t count, uint16_t *data);

ssize_t utf16_to_utf8(const uint16_t *utf16, size_t count, uint8_t *utf8);

#endif
