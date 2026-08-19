/*
 * random_check_image.c
 *
 * Random-access verifier for the deterministic IDE test image.
 *
 * Usage:
 *   ./random_check_image image.img [iterations] [seed]
 *
 * Examples:
 *   ./random_check_image test.img
 *   ./random_check_image test.img 100000
 *   ./random_check_image test.img 100000 0x12345678
 */

#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// AI generated

#define DEFAULT_ITERATIONS 100000
#define MAX_READ_SIZE      (1024 * 1024)
#define SECTOR_SIZE        512

#define MAX_REPORTED_ERRORS 20

/*
 * Must match make-image.c.
 */
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

/*
 * Simple deterministic PRNG.
 *
 * The seed is printed on startup so a failing test can be reproduced.
 */
static uint64_t rng_state;

static uint64_t rng_next(void)
{
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;

    return rng_state * UINT64_C(2685821657736338717);
}

static uint64_t rng_range(uint64_t max)
{
    if (max == 0)
        return 0;

    return rng_next() % max;
}

static uint64_t file_size(FILE *f)
{
    if (fseek(f, 0, SEEK_END) != 0)
        return 0;

    off_t size = ftell(f);

    if (size < 0)
        return 0;

    if (fseek(f, 0, SEEK_SET) != 0)
        return 0;

    return (uint64_t)size;
}

static int verify_buffer(const uint8_t *buf,
                         size_t size,
                         uint64_t offset)
{
    for (size_t i = 0; i < size; i++) {
        uint8_t expected = pattern_byte(offset + i);

        if (buf[i] != expected) {
            fprintf(stderr,
                    "\nMISMATCH:\n"
                    "  offset:   0x%" PRIx64 " (%" PRIu64 ")\n"
                    "  got:      0x%02x\n"
                    "  expected: 0x%02x\n",
                    offset + i,
                    offset + i,
                    buf[i],
                    expected);

            return -1;
        }
    }

    return 0;
}

static int do_read(FILE *f,
                   uint8_t *buf,
                   size_t size,
                   uint64_t offset)
{
    if (fseek(f, (off_t)offset, SEEK_SET) != 0) {
        fprintf(stderr,
                "\nfseeko failed at offset %" PRIu64 ": %s\n",
                offset,
                strerror(errno));
        return -1;
    }

    size_t got = fread(buf, 1, size, f);

    if (got != size) {
        fprintf(stderr,
                "\nshort read:\n"
                "  offset: %" PRIu64 "\n"
                "  wanted: %zu\n"
                "  got:    %zu\n",
                offset,
                size,
                got);

        if (ferror(f))
            fprintf(stderr, "  error: %s\n", strerror(errno));

        return -1;
    }

    return verify_buffer(buf, size, offset);
}

/*
 * Choose interesting offsets in addition to purely random ones.
 */
static uint64_t choose_offset(uint64_t image_size,
                              size_t read_size)
{
    uint64_t max_offset = image_size - read_size;

    switch (rng_next() % 10) {

    /*
     * Completely random.
     */
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        return rng_range(max_offset + 1);

    /*
     * Sector aligned.
     */
    case 5:
        return (rng_range((max_offset / SECTOR_SIZE) + 1))
               * SECTOR_SIZE;

    /*
     * One byte before a sector boundary.
     */
    case 6: {
        if (max_offset == 0)
            return 0;

        uint64_t sectors = max_offset / SECTOR_SIZE;

        if (sectors == 0)
            return rng_range(max_offset + 1);

        uint64_t sector = rng_range(sectors);

        uint64_t offset = sector * SECTOR_SIZE;

        if (offset > 0)
            offset--;

        if (offset > max_offset)
            offset = max_offset;

        return offset;
    }

    /*
     * One byte after a sector boundary.
     */
    case 7: {
        uint64_t sectors = max_offset / SECTOR_SIZE;

        if (sectors == 0)
            return rng_range(max_offset + 1);

        uint64_t offset =
            rng_range(sectors) * SECTOR_SIZE + 1;

        if (offset > max_offset)
            offset = max_offset;

        return offset;
    }

    /*
     * Near the beginning.
     */
    case 8:
        return rng_range(
            max_offset < 4096 ? max_offset + 1 : 4096);

    /*
     * Near the end.
     */
    default:
        if (max_offset < 4096)
            return rng_range(max_offset + 1);

        return max_offset - rng_range(4096);
    }
}

static size_t choose_read_size(uint64_t image_size)
{
    uint64_t remaining = image_size;

    /*
     * We want lots of awkward sizes.
     */
    static const size_t interesting[] = {
        1,
        2,
        3,
        4,
        7,
        8,
        15,
        16,
        31,
        32,
        63,
        64,
        127,
        128,
        255,
        256,
        511,
        512,
        513,
        1023,
        1024,
        1025,
        2047,
        2048,
        4095,
        4096,
        4097,
        8191,
        8192,
        8193,
        16384,
        32768,
        65536,
        131072,
        262144,
        524288,
        1048576
    };

    if (remaining == 0)
        return 0;

    if ((rng_next() % 10) < 7) {
        size_t size =
            interesting[rng_range(
                sizeof(interesting) / sizeof(interesting[0]))];

        if ((uint64_t)size <= remaining)
            return size;
    }

    /*
     * Random size.
     */
    uint64_t size = 1 + rng_range(
        remaining < MAX_READ_SIZE
            ? remaining
            : MAX_READ_SIZE);

    return (size_t)size;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 4) {
        fprintf(stderr,
                "usage: %s <image> [iterations] [seed]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argv[1];

    uint64_t iterations = DEFAULT_ITERATIONS;

    if (argc >= 3) {
        char *end;

        errno = 0;
        iterations = strtoull(argv[2], &end, 0);

        if (errno || *end != '\0' || iterations == 0) {
            fprintf(stderr, "invalid iteration count: %s\n",
                    argv[2]);
            return EXIT_FAILURE;
        }
    }

    if (argc >= 4) {
        char *end;

        errno = 0;
        rng_state = strtoull(argv[3], &end, 0);

        if (errno || *end != '\0' || rng_state == 0) {
            fprintf(stderr, "invalid seed: %s\n",
                    argv[3]);
            return EXIT_FAILURE;
        }
    } else {
        /*
         * Fixed default is intentional: failures are reproducible.
         */
        rng_state = UINT64_C(0x123456789abcdef0);
    }

    FILE *f = fopen(path, "rb");

    if (!f) {
        fprintf(stderr,
                "fopen(%s): %s\n",
                path,
                strerror(errno));
        return EXIT_FAILURE;
    }

    uint64_t image_size = file_size(f);

    if (image_size == 0) {
        fprintf(stderr, "could not determine image size\n");
        fclose(f);
        return EXIT_FAILURE;
    }

    uint8_t *buf = malloc(MAX_READ_SIZE);

    if (!buf) {
        fprintf(stderr,
                "malloc(%u): %s\n",
                MAX_READ_SIZE,
                strerror(errno));
        fclose(f);
        return EXIT_FAILURE;
    }

    printf("image:      %s\n", path);
    printf("size:       %" PRIu64 " bytes (%" PRIu64 " MiB)\n",
           image_size,
           image_size / (1024 * 1024));
    printf("iterations: %" PRIu64 "\n", iterations);
    printf("seed:       0x%016" PRIx64 "\n", rng_state);
    printf("\n");

    uint64_t total_bytes = 0;

    for (uint64_t test = 0; test < iterations; test++) {

        size_t size = choose_read_size(image_size);

        uint64_t offset =
            choose_offset(image_size, size);

        printf("\rtest %" PRIu64 "/%" PRIu64
               "  offset=0x%" PRIx64
               "  size=%zu       ",
               test + 1,
               iterations,
               offset,
               size);

        fflush(stdout);

        if (do_read(f, buf, size, offset) < 0) {
            fprintf(stderr,
                    "\n\nFAILED\n"
                    "seed:   0x%016" PRIx64 "\n"
                    "test:   %" PRIu64 "\n"
                    "offset: 0x%" PRIx64 "\n"
                    "size:   %zu\n",
                    rng_state,
                    test,
                    offset,
                    size);

            free(buf);
            fclose(f);
            return EXIT_FAILURE;
        }

        total_bytes += size;
    }

    printf("\n\nPASS\n");
    printf("tests:       %" PRIu64 "\n", iterations);
    printf("bytes read:  %" PRIu64 "\n", total_bytes);

    free(buf);
    fclose(f);

    return EXIT_SUCCESS;
}