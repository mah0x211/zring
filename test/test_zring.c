/*
 *  Copyright (C) 2026 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <string.h>

#include "zring.h"

/* ---- test runner --------------------------------------------------------- */

static int g_total_failures = 0;
static int g_test_failures  = 0;

/**
 * CHECK(cond) — assert a condition; on failure print file/line and increment
 * both the per-test and global failure counters.
 */
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "  FAIL  %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            g_test_failures++;                                                 \
            g_total_failures++;                                                \
        }                                                                      \
    } while (0)

/**
 * run_test() — execute a single test function and print pass/fail.
 */
static void run_test(const char *name, void (*fn)(void))
{
    g_test_failures = 0;
    fn();
    if (g_test_failures == 0) {
        printf("pass  %s\n", name);
    } else {
        printf("FAIL  %s  (%d check(s) failed)\n", name, g_test_failures);
    }
}

/* ---- tests -------------------------------------------------------------- */

static void test_init(void)
{
    char    mem[8];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    CHECK(rb.cap   == 8);
    CHECK(rb.head  == 0);
    CHECK(rb.tail  == 0);
    CHECK(rb.count == 0);
    CHECK(rb.mem   == mem);
}

static void test_empty(void)
{
    char    mem[8];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    size_t len = 99;
    void  *ptr = zring_data(&rb, &len);
    CHECK(ptr == NULL);
    CHECK(len == 0);
}

static void test_full(void)
{
    char    mem[4];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* fill the buffer */
    size_t len;
    void  *ptr = zring_space(&rb, &len);
    CHECK(ptr != NULL);
    CHECK(len == 4);
    memcpy(ptr, "ABCD", 4);
    CHECK(zring_commit(&rb, 4) == 0);
    CHECK(rb.count == 4);
    CHECK(rb.tail  == 0); /* wrapped back to 0 */

    /* no more space */
    ptr = zring_space(&rb, &len);
    CHECK(ptr == NULL);
    CHECK(len == 0);
}

static void test_linear_write_read(void)
{
    char    mem[8];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* write 3 bytes */
    size_t len;
    char  *ptr = zring_space(&rb, &len);
    CHECK(ptr == mem);
    CHECK(len == 8);
    memcpy(ptr, "abc", 3);
    CHECK(zring_commit(&rb, 3) == 0);
    CHECK(rb.count == 3);
    CHECK(rb.tail  == 3);

    /* read 3 bytes */
    ptr = zring_data(&rb, &len);
    CHECK(ptr == mem);
    CHECK(len == 3);
    CHECK(memcmp(ptr, "abc", 3) == 0);
    CHECK(zring_consume(&rb, 3) == 0);
    CHECK(rb.count == 0);
    CHECK(rb.head  == 3);
}

static void test_wraparound(void)
{
    char    mem[8];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* advance head and tail to position 6 */
    size_t len;
    void  *ptr = zring_space(&rb, &len);
    memset(ptr, 'X', 6);
    CHECK(zring_commit(&rb, 6) == 0);
    ptr = zring_data(&rb, &len);
    CHECK(zring_consume(&rb, 6) == 0);
    CHECK(rb.head == 6);
    CHECK(rb.tail == 6);

    /*
     * Write 2 bytes: tail is at 6, cap is 8.
     * Contiguous space is 2 bytes (indices 6-7).
     */
    ptr = zring_space(&rb, &len);
    CHECK((char *)ptr == mem + 6);
    CHECK(len == 2);
    memcpy(ptr, "AB", 2);
    CHECK(zring_commit(&rb, 2) == 0);
    CHECK(rb.tail  == 0); /* wrapped */
    CHECK(rb.count == 2);

    /* Space now starts at 0 (wrapped), 6 bytes available contiguously */
    ptr = zring_space(&rb, &len);
    CHECK((char *)ptr == mem);
    CHECK(len == 6);
    memcpy(ptr, "CD", 2);
    CHECK(zring_commit(&rb, 2) == 0);
    CHECK(rb.tail  == 2);
    CHECK(rb.count == 4);

    /* Data starts at head=6, 2 bytes before wrap */
    ptr = zring_data(&rb, &len);
    CHECK((char *)ptr == mem + 6);
    CHECK(len == 2);
    CHECK(memcmp(ptr, "AB", 2) == 0);
    CHECK(zring_consume(&rb, 2) == 0);
    CHECK(rb.head  == 0); /* wrapped */
    CHECK(rb.count == 2);

    /* Remaining data starts at 0 */
    ptr = zring_data(&rb, &len);
    CHECK((char *)ptr == mem);
    CHECK(len == 2);
    CHECK(memcmp(ptr, "CD", 2) == 0);
    CHECK(zring_consume(&rb, 2) == 0);
    CHECK(rb.count == 0);
}

static void test_partial_space_at_wrap(void)
{
    char    mem[4];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* fill 3 bytes then consume 3 so head=tail=3 */
    size_t len;
    void  *ptr = zring_space(&rb, &len);
    CHECK(zring_commit(&rb, 3) == 0);
    ptr = zring_data(&rb, &len);
    CHECK(zring_consume(&rb, 3) == 0);
    CHECK(rb.head  == 3);
    CHECK(rb.tail  == 3);
    CHECK(rb.count == 0);

    /*
     * Space: tail=3, cap=4 → contig=1. avail=4. min(4,1)=1.
     * Only 1 byte returned for the segment that fits before the end.
     */
    ptr = zring_space(&rb, &len);
    CHECK((char *)ptr == mem + 3);
    CHECK(len == 1);
    memcpy(ptr, "X", 1);
    CHECK(zring_commit(&rb, 1) == 0);

    /* wrap: tail=0, count=1 */
    ptr = zring_space(&rb, &len);
    CHECK((char *)ptr == mem);
    CHECK(len == 3);
}

static void test_commit_zero(void)
{
    char    mem[4];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    CHECK(zring_commit(&rb, 0) == 0);
    CHECK(rb.tail  == 0);
    CHECK(rb.count == 0);
}

static void test_consume_zero(void)
{
    char    mem[4];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    CHECK(zring_consume(&rb, 0) == 0);
    CHECK(rb.head  == 0);
    CHECK(rb.count == 0);
}

static void test_commit_overflow(void)
{
    char    mem[4];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* commit more than capacity must fail without mutating state */
    CHECK(zring_commit(&rb, 5) == -1);
    CHECK(rb.tail  == 0);
    CHECK(rb.count == 0);

    /* fill 3 bytes, then try to commit 2 more (only 1 contiguous byte free) */
    CHECK(zring_commit(&rb, 3) == 0);
    CHECK(zring_commit(&rb, 2) == -1);
    CHECK(rb.count == 3);
    CHECK(rb.tail  == 3);

    /* total free = 1; n=1 fits within the remaining contiguous byte */
    CHECK(zring_commit(&rb, 1) == 0);
    CHECK(rb.count == 4);

    /*
     * head=2, tail=2, count=0: contig = cap-tail = 2.
     * Committing 3 (> 2) must fail even though 3 <= cap.
     */
    zring_init(&rb, mem, sizeof(mem));
    CHECK(zring_commit(&rb, 2) == 0);  /* tail=2 */
    CHECK(zring_consume(&rb, 2) == 0); /* head=2, tail=2, count=0 */
    CHECK(zring_commit(&rb, 3) == -1);
    CHECK(rb.count == 0);
    CHECK(rb.tail  == 2);
}

static void test_consume_overflow(void)
{
    char    mem[4];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* consume from empty buffer must fail */
    CHECK(zring_consume(&rb, 1) == -1);
    CHECK(rb.head  == 0);
    CHECK(rb.count == 0);

    /* store 2 bytes, then try to consume 3 (> count) */
    CHECK(zring_commit(&rb, 2) == 0);
    CHECK(zring_consume(&rb, 3) == -1);
    CHECK(rb.count == 2);
    CHECK(rb.head  == 0);

    /*
     * head=2, tail=1, count=3 (data at [2],[3],[0]).
     * zring_data() returns min(count=3, contig=cap-head=2) = 2 bytes.
     * Consuming 3 (> 2 contiguous) must fail.
     */
    zring_init(&rb, mem, sizeof(mem));
    CHECK(zring_commit(&rb, 2) == 0);  /* tail=2 */
    CHECK(zring_consume(&rb, 2) == 0); /* head=2 */
    CHECK(zring_commit(&rb, 2) == 0);  /* tail=0 (wrap) */
    CHECK(zring_commit(&rb, 1) == 0);  /* tail=1 */
    CHECK(rb.head  == 2);
    CHECK(rb.tail  == 1);
    CHECK(rb.count == 3);
    CHECK(zring_consume(&rb, 3) == -1); /* 3 > contig=2 */
    CHECK(rb.count == 3);
    CHECK(rb.head  == 2);
}

static void test_size_helpers(void)
{
    char    mem[8];
    zring_t rb;
    zring_init(&rb, mem, sizeof(mem));

    /* empty: head=0, tail=0, count=0, cap=8 */
    /* avail=8, contig=8 → min(8,8)=8 (avail < contig = false → contig) */
    CHECK(zring_space_size(&rb) == 8);
    /* count=0, contig=8 → min(0,8)=0 (count < contig = true → count) */
    CHECK(zring_data_size(&rb)  == 0);

    /* after commit(3): head=0, tail=3, count=3 */
    CHECK(zring_commit(&rb, 3) == 0);
    /* avail=5, contig=5 → min(5,5)=5 (avail < contig = false → contig) */
    CHECK(zring_space_size(&rb) == 5);
    /* count=3, contig=8 → min(3,8)=3 (count < contig = true → count) */
    CHECK(zring_data_size(&rb)  == 3);

    /* after consume(3): head=3, tail=3, count=0 */
    CHECK(zring_consume(&rb, 3) == 0);
    /* avail=8, contig=5 → min(8,5)=5 (avail < contig = false → contig) */
    CHECK(zring_space_size(&rb) == 5);
    /* count=0, contig=5 → min(0,5)=0 (count < contig = true → count) */
    CHECK(zring_data_size(&rb)  == 0);

    /* commit(5): head=3, tail=0 (=3+5 mod 8), count=5 (tail wrapped) */
    CHECK(zring_commit(&rb, 5) == 0);
    /* avail=3, contig=8 → min(3,8)=3 (avail < contig = true → avail) */
    CHECK(zring_space_size(&rb) == 3);
    /* count=5, contig=5 → min(5,5)=5 (count < contig = false → contig) */
    CHECK(zring_data_size(&rb)  == 5);
}

/* ---- main --------------------------------------------------------------- */

int main(void)
{
    run_test("init",                        test_init);
    run_test("empty: data returns NULL",    test_empty);
    run_test("full: space returns NULL",    test_full);
    run_test("linear write then read",      test_linear_write_read);
    run_test("wrap-around write and read",  test_wraparound);
    run_test("partial space at wrap",       test_partial_space_at_wrap);
    run_test("commit(0) is a no-op",        test_commit_zero);
    run_test("consume(0) is a no-op",       test_consume_zero);
    run_test("commit overflow returns -1",  test_commit_overflow);
    run_test("consume overflow returns -1", test_consume_overflow);
    run_test("space_size and data_size",    test_size_helpers);

    if (g_total_failures) {
        fprintf(stderr, "\n%d check(s) failed\n", g_total_failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
