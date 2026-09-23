/*
 * allocator_stress.c
 *
 * Stress test for:
 *   malloc
 *   free
 *   calloc
 *   realloc
 *   posix_memalign
 *
 * Build with something like:
 *
 *   cc -O0 -g -fsanitize=address,undefined allocator_stress.c allocator.o -o allocator_stress
 *
 * If your allocator replaces libc malloc/free, you may want to omit
 * sanitizers if they interfere with your allocator.
 *
 * Usage:
 *   ./allocator_stress
 *   ./allocator_stress 12345
 */

// AI generated

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define SLOTS              4096
#define OPERATIONS         5000000
#define MAX_ALLOCATION     (1024 * 1024)
#define VERIFY_BYTES       256

/*
 * Keep the random generator deterministic.
 * This makes crashes reproducible.
 */
static uint64_t rng_state = 0x123456789abcdef0ULL;

static uint64_t
rng64(void)
{
    uint64_t x = rng_state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;

    rng_state = x;
    return x * 2685821657736338717ULL;
}

static size_t
random_size(void)
{
    uint64_t r = rng64();

    /*
     * Bias heavily toward small allocations because those exercise
     * the slab bins, while still occasionally producing large ones.
     */
    switch (r & 15) {
    case 0:
        return 1 + (rng64() % 16);

    case 1:
        return 1 + (rng64() % 32);

    case 2:
        return 1 + (rng64() % 64);

    case 3:
        return 1 + (rng64() % 128);

    case 4:
        return 1 + (rng64() % 256);

    case 5:
        return 1 + (rng64() % 512);

    case 6:
        return 1 + (rng64() % 1024);

    case 7:
        return 1 + (rng64() % 2048);

    case 8:
        return 1 + (rng64() % 4096);

    case 9:
        return 1 + (rng64() % 16384);

    case 10:
        return 1 + (rng64() % 65536);

    case 11:
        return 1 + (rng64() % 262144);

    case 12:
        return 1 + (rng64() % MAX_ALLOCATION);

    default:
        /*
         * Hit exact interesting allocator boundaries frequently.
         */
        switch (rng64() % 20) {
        case 0: return 1;
        case 1: return 15;
        case 2: return 16;
        case 3: return 17;
        case 4: return 31;
        case 5: return 32;
        case 6: return 33;
        case 7: return 47;
        case 8: return 48;
        case 9: return 49;
        case 10: return 63;
        case 11: return 64;
        case 12: return 65;
        case 13: return 95;
        case 14: return 96;
        case 15: return 97;
        case 16: return 127;
        case 17: return 128;
        case 18: return 129;
        default: return 2049;
        }
    }
}

struct allocation {
    void *ptr;
    size_t size;
    uint64_t pattern;
    int kind;
};

static struct allocation slots[SLOTS];

static unsigned char
pattern_byte(uint64_t pattern, size_t index)
{
    uint64_t x = pattern;

    x ^= index * 0x9e3779b97f4a7c15ULL;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;

    return (unsigned char)x;
}

static void
fill_pattern(void *ptr, size_t size, uint64_t pattern)
{
    unsigned char *p = ptr;

    for (size_t i = 0; i < size; i++)
        p[i] = pattern_byte(pattern, i);
}

static void
check_pattern(const struct allocation *a)
{
    unsigned char *p = a->ptr;

    /*
     * Checking the entire allocation is expensive for huge allocations.
     * Check the beginning and end, plus a random-ish selection.
     */
    size_t n = a->size < VERIFY_BYTES ? a->size : VERIFY_BYTES;

    for (size_t i = 0; i < n; i++) {
        unsigned char expected = pattern_byte(a->pattern, i);

        if (p[i] != expected) {
            fprintf(stderr,
                    "\nCORRUPTION!\n"
                    "  ptr      = %p\n"
                    "  size     = %zu\n"
                    "  index    = %zu\n"
                    "  expected = %02x\n"
                    "  actual   = %02x\n",
                    a->ptr,
                    a->size,
                    i,
                    expected,
                    p[i]);

            abort();
        }
    }

    if (a->size > VERIFY_BYTES) {
        size_t start = a->size - VERIFY_BYTES;

        for (size_t i = start; i < a->size; i++) {
            unsigned char expected = pattern_byte(a->pattern, i);

            if (p[i] != expected) {
                fprintf(stderr,
                        "\nCORRUPTION!\n"
                        "  ptr      = %p\n"
                        "  size     = %zu\n"
                        "  index    = %zu\n"
                        "  expected = %02x\n"
                        "  actual   = %02x\n",
                        a->ptr,
                        a->size,
                        i,
                        expected,
                        p[i]);

                abort();
            }
        }
    }

    /*
     * A few pseudo-random positions.
     */
    for (int i = 0; i < 16 && a->size; i++) {
        size_t pos = rng64() % a->size;

        unsigned char expected = pattern_byte(a->pattern, pos);

        if (p[pos] != expected) {
            fprintf(stderr,
                    "\nCORRUPTION!\n"
                    "  ptr      = %p\n"
                    "  size     = %zu\n"
                    "  index    = %zu\n"
                    "  expected = %02x\n"
                    "  actual   = %02x\n",
                    a->ptr,
                    a->size,
                    pos,
                    expected,
                    p[pos]);

            abort();
        }
    }
}

static void
test_malloc(void)
{
    size_t size = random_size();

    void *p = malloc(size);

    if (!p) {
        fprintf(stderr, "malloc(%zu) failed\n", size);
        abort();
    }

    /*
     * malloc should provide at least 16-byte alignment on the allocator
     * being tested.
     */
    if ((uintptr_t)p % 16 != 0) {
        fprintf(stderr,
                "BAD ALIGNMENT: malloc(%zu) returned %p\n",
                size, p);
        abort();
    }

    struct allocation a = {
        .ptr = p,
        .size = size,
        .pattern = rng64(),
        .kind = 0
    };

    fill_pattern(p, size, a.pattern);
    check_pattern(&a);

    free(p);
}

static void
test_calloc(void)
{
    size_t nmemb;
    size_t size;

    switch (rng64() % 5) {
    case 0:
        nmemb = 1;
        size = random_size();
        break;

    case 1:
        nmemb = 2 + rng64() % 16;
        size = 1 + rng64() % 256;
        break;

    case 2:
        nmemb = 1 + rng64() % 64;
        size = 1 + rng64() % 1024;
        break;

    case 3:
        nmemb = 1 + rng64() % 1024;
        size = 1 + rng64() % 1024;
        break;

    default:
        nmemb = 1 + rng64() % 16;
        size = 1 + rng64() % 16384;
        break;
    }

    size_t total = nmemb * size;

    void *p = calloc(nmemb, size);

    if (!p) {
        fprintf(stderr,
                "calloc(%zu, %zu) failed\n",
                nmemb, size);
        abort();
    }

    if ((uintptr_t)p % 16 != 0) {
        fprintf(stderr,
                "BAD ALIGNMENT: calloc returned %p\n",
                p);
        abort();
    }

    unsigned char *bytes = p;

    /*
     * Verify zero initialization.
     */
    for (size_t i = 0; i < total; i++) {
        if (bytes[i] != 0) {
            fprintf(stderr,
                    "\nCALLOC CORRUPTION!\n"
                    "  ptr   = %p\n"
                    "  total = %zu\n"
                    "  index = %zu\n"
                    "  value = %02x\n",
                    p, total, i, bytes[i]);

            abort();
        }
    }

    /*
     * Write to it too, so the allocator doesn't only get tested
     * with freshly-zeroed memory.
     */
    memset(p, 0xa5, total);

    free(p);
}

static void
test_posix_memalign(void)
{
    static const size_t alignments[] = {
        8,
        16,
        32,
        64,
        128,
        256,
        512,
        1024,
        4096,
        8192,
        16384,
        65536
    };

    size_t alignment =
        alignments[rng64() %
                   (sizeof(alignments) / sizeof(alignments[0]))];

    size_t size = random_size();

    void *p = (void *)(uintptr_t)0xdeadbeef;

    int ret = posix_memalign(&p, alignment, size);

    if (ret != 0) {
        fprintf(stderr,
                "posix_memalign(%zu, %zu) returned %d\n",
                alignment, size, ret);
        abort();
    }

    if (!p) {
        fprintf(stderr,
                "posix_memalign returned NULL with success\n");
        abort();
    }

    if ((uintptr_t)p % alignment != 0) {
        fprintf(stderr,
                "\nBAD POSIX_MEMALIGN ALIGNMENT!\n"
                "  alignment = %zu\n"
                "  ptr       = %p\n",
                alignment, p);
        abort();
    }

    struct allocation a = {
        .ptr = p,
        .size = size,
        .pattern = rng64(),
        .kind = 2
    };

    fill_pattern(p, size, a.pattern);
    check_pattern(&a);

    free(p);
}

static void
test_realloc(struct allocation *a)
{
    check_pattern(a);

    size_t old_size = a->size;
    size_t new_size = random_size();

    void *old_ptr = a->ptr;

    void *p = realloc(old_ptr, new_size);

    if (!p) {
        /*
         * realloc failure must leave the original allocation intact.
         */
        check_pattern(a);
        return;
    }

    /*
     * realloc must preserve min(old_size, new_size) bytes.
     */
    size_t preserved = old_size < new_size ? old_size : new_size;

    unsigned char *bytes = p;

    for (size_t i = 0; i < preserved; i++) {
        unsigned char expected = pattern_byte(a->pattern, i);

        if (bytes[i] != expected) {
            fprintf(stderr,
                    "\nREALLOC CORRUPTION!\n"
                    "  old ptr  = %p\n"
                    "  new ptr  = %p\n"
                    "  old size = %zu\n"
                    "  new size = %zu\n"
                    "  index    = %zu\n"
                    "  expected = %02x\n"
                    "  actual   = %02x\n",
                    old_ptr,
                    p,
                    old_size,
                    new_size,
                    i,
                    expected,
                    bytes[i]);

            abort();
        }
    }

    /*
     * Use a new pattern after realloc so later corruption can be
     * distinguished from the old contents.
     */
    a->ptr = p;
    a->size = new_size;
    a->pattern = rng64();

    fill_pattern(p, new_size, a->pattern);
}

static int
find_free_slot(void)
{
    for (int tries = 0; tries < 32; tries++) {
        size_t i = rng64() % SLOTS;

        if (!slots[i].ptr)
            return (int)i;
    }

    for (size_t i = 0; i < SLOTS; i++)
        if (!slots[i].ptr)
            return (int)i;

    return -1;
}

static int
find_used_slot(void)
{
    for (int tries = 0; tries < 32; tries++) {
        size_t i = rng64() % SLOTS;

        if (slots[i].ptr)
            return (int)i;
    }

    for (size_t i = 0; i < SLOTS; i++)
        if (slots[i].ptr)
            return (int)i;

    return -1;
}

static void
do_random_allocation(void)
{
    int slot = find_free_slot();

    if (slot < 0)
        return;

    uint64_t operation = rng64() % 100;

    /*
     * 45% malloc
     * 25% calloc
     * 30% posix_memalign
     */
    if (operation < 45) {
        size_t size = random_size();

        void *p = malloc(size);

        if (!p) {
            fprintf(stderr, "malloc(%zu) failed\n", size);
            abort();
        }

        if ((uintptr_t)p % 16 != 0) {
            fprintf(stderr,
                    "malloc returned badly aligned pointer %p\n",
                    p);
            abort();
        }

        slots[slot].ptr = p;
        slots[slot].size = size;
        slots[slot].pattern = rng64();
        slots[slot].kind = 0;

        fill_pattern(p, size, slots[slot].pattern);
    }
    else if (operation < 70) {
        size_t nmemb = 1 + rng64() % 64;
        size_t size = 1 + rng64() % 4096;
        size_t total = nmemb * size;

        void *p = calloc(nmemb, size);

        if (!p) {
            fprintf(stderr,
                    "calloc(%zu, %zu) failed\n",
                    nmemb, size);
            abort();
        }

        unsigned char *bytes = p;

        /*
         * Full verification for these relatively small allocations.
         */
        for (size_t i = 0; i < total; i++) {
            if (bytes[i] != 0) {
                fprintf(stderr,
                        "calloc returned non-zero memory at %zu\n",
                        i);
                abort();
            }
        }

        slots[slot].ptr = p;
        slots[slot].size = total;
        slots[slot].pattern = rng64();
        slots[slot].kind = 1;

        fill_pattern(p, total, slots[slot].pattern);
    }
    else {
        static const size_t alignments[] = {
            8, 16, 32, 64, 128, 256, 512, 1024, 4096, 16384
        };

        size_t alignment =
            alignments[rng64() %
                       (sizeof(alignments) / sizeof(alignments[0]))];

        size_t size = random_size();

        void *p = NULL;

        int ret = posix_memalign(&p, alignment, size);

        if (ret != 0) {
            fprintf(stderr,
                    "posix_memalign failed: alignment=%zu size=%zu ret=%d\n",
                    alignment, size, ret);
            abort();
        }

        if (!p || (uintptr_t)p % alignment != 0) {
            fprintf(stderr,
                    "bad posix_memalign result: ptr=%p alignment=%zu\n",
                    p, alignment);
            abort();
        }

        slots[slot].ptr = p;
        slots[slot].size = size;
        slots[slot].pattern = rng64();
        slots[slot].kind = 2;

        fill_pattern(p, size, slots[slot].pattern);
    }
}

static void
do_random_free(void)
{
    int slot = find_used_slot();

    if (slot < 0)
        return;

    check_pattern(&slots[slot]);

    free(slots[slot].ptr);

    memset(&slots[slot], 0, sizeof(slots[slot]));
}

static void
do_random_realloc(void)
{
    int slot = find_used_slot();

    if (slot < 0)
        return;

    test_realloc(&slots[slot]);
}

static void
verify_all(void)
{
    for (size_t i = 0; i < SLOTS; i++) {
        if (slots[i].ptr)
            check_pattern(&slots[i]);
    }
}

static void
free_all(void)
{
    for (size_t i = 0; i < SLOTS; i++) {
        if (slots[i].ptr) {
            check_pattern(&slots[i]);
            free(slots[i].ptr);
            slots[i].ptr = NULL;
        }
    }
}

int
main(int argc, char **argv)
{
    uint64_t seed = 0x123456789abcdef0ULL;

    if (argc > 1)
        seed = strtoull(argv[1], NULL, 0);

    rng_state = seed;

    printf("allocator stress test\n");
    printf("  seed       = 0x%016llx\n",
           (unsigned long long)seed);
    printf("  slots      = %d\n", SLOTS);
    printf("  operations = %d\n", OPERATIONS);
    fflush(stdout);

    /*
     * First run some targeted tests.
     */
    printf("targeted malloc...\n");
    for (int i = 0; i < 10000; i++)
        test_malloc();

    printf("targeted calloc...\n");
    for (int i = 0; i < 10000; i++)
        test_calloc();

    printf("targeted posix_memalign...\n");
    for (int i = 0; i < 10000; i++)
        test_posix_memalign();

    /*
     * Main randomized stress test.
     */
    printf("random stress...\n");
    fflush(stdout);

    for (size_t op = 0; op < OPERATIONS; op++) {
        uint64_t r = rng64() % 100;

        if (r < 35) {
            /*
             * Allocation.
             */
            do_random_allocation();
        }
        else if (r < 60) {
            /*
             * Free.
             */
            do_random_free();
        }
        else if (r < 90) {
            /*
             * Realloc.
             */
            do_random_realloc();
        }
        else {
            /*
             * Verify everything.
             */
            verify_all();
        }

        if ((op % 100000) == 0) {
            printf("\r%zu / %d", op, OPERATIONS);
            fflush(stdout);
        }
    }

    printf("\nfinal verification...\n");
    verify_all();

    printf("freeing everything...\n");
    free_all();

    printf("PASSED\n");

    return 0;
}
