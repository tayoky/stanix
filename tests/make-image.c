#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AI generated

#define CHUNK_SIZE (1024 * 1024)

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

static int write_all(FILE *f, const void *buf, size_t size)
{
    const uint8_t *p = buf;

    while (size != 0) {
        size_t n = fwrite(p, 1, size, f);
        if (n == 0) {
            if (ferror(f))
                return -1;
            return -1;
        }

        p += n;
        size -= n;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <image> <size-in-mib>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argv[1];

    char *end;
    errno = 0;
    unsigned long long mib = strtoull(argv[2], &end, 10);

    if (errno || *end != '\0' || mib == 0) {
        fprintf(stderr, "invalid size: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    uint64_t total_size = (uint64_t)mib * 1024 * 1024;

    FILE *f = fopen(path, "wb");
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

    while (offset < total_size) {
        size_t n = CHUNK_SIZE;

        if (total_size - offset < n)
            n = (size_t)(total_size - offset);

        for (size_t i = 0; i < n; i++)
            buf[i] = pattern_byte(offset + i);

        if (write_all(f, buf, n) < 0) {
            fprintf(stderr, "write at offset %" PRIu64 ": %s\n",
                    offset, strerror(errno));
            free(buf);
            fclose(f);
            return EXIT_FAILURE;
        }

        offset += n;

        fprintf(stderr,
                "\rgenerated %" PRIu64 " / %" PRIu64 " MiB",
                offset / (1024 * 1024),
                total_size / (1024 * 1024));
    }

    fprintf(stderr, "\n");

    free(buf);

    if (fclose(f) != 0) {
        fprintf(stderr, "fclose: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}