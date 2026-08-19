#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AI generated

#define CHUNK_SIZE (1024 * 1024)
#define MAX_REPORTED_ERRORS 32

static uint64_t mix64(uint64_t x)
{
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static uint8_t pattern_byte(uint64_t offset)
{
    uint64_t x = mix64(offset >> 3);
    return (uint8_t)(x >> ((offset & 7) * 8));
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <image>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argv[1];

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }

    uint8_t *buf = malloc(CHUNK_SIZE);
    if (!buf) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        fclose(f);
        return EXIT_FAILURE;
    }

    uint64_t offset = 0;
    uint64_t errors = 0;
    uint64_t bytes = 0;

    for (;;) {
        size_t n = fread(buf, 1, CHUNK_SIZE, f);

        if (n == 0) {
            if (ferror(f)) {
                fprintf(stderr, "read at offset %" PRIu64 ": %s\n",
                        offset, strerror(errno));
                free(buf);
                fclose(f);
                return EXIT_FAILURE;
            }

            break;
        }

        for (size_t i = 0; i < n; i++) {
            uint8_t expected = pattern_byte(offset + i);

            if (buf[i] != expected) {
                errors++;

                if (errors <= MAX_REPORTED_ERRORS) {
                    fprintf(stderr,
                            "mismatch at offset 0x%" PRIx64
                            " (%" PRIu64 "): got 0x%02x, expected 0x%02x\n",
                            offset + i,
                            offset + i,
                            buf[i],
                            expected);
                }
            }
        }

        offset += n;
        bytes += n;

        fprintf(stderr,
                "\rchecked %" PRIu64 " MiB",
                bytes / (1024 * 1024));
    }

    fprintf(stderr, "\n");

    if (errors != 0) {
        fprintf(stderr,
                "FAILED: %" PRIu64 " mismatched bytes"
                " (first %d shown)\n",
                errors,
                MAX_REPORTED_ERRORS);

        free(buf);
        fclose(f);
        return EXIT_FAILURE;
    }

    fprintf(stderr,
            "OK: %" PRIu64 " bytes verified successfully\n",
            bytes);

    free(buf);
    fclose(f);

    return EXIT_SUCCESS;
}