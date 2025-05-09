/* teste.c – edge‑case battery without fancy initialisers */
#include <stdio.h>
#include <stdint.h>     /* int32_t */
#include <string.h>     /* memset, strcpy */
#include <stdlib.h>     /* exit */
#include "gravacomp.h"


/* ---------- scenario structs --------------------------------------- */
struct A { int32_t si; };
struct B { unsigned u; };
struct C { char s[64]; };
struct D { int32_t si; unsigned u; };
struct E { int32_t si; char s[2]; };

static void run_test(const char *title,
                     int n,
                     void *base,
                     const char *desc)
{
    printf("\n=== %s ===\n", title);

    FILE *f = fopen("tmp.bin", "wb");
    if (!f) { perror("fopen"); exit(1); }
    if (gravacomp(n, base, (char *)desc, f) != 0) {
        puts("gravacomp erro!"); exit(1);
    }
    fclose(f);

    f = fopen("tmp.bin", "rb");
    if (!f) { perror("fopen"); exit(1); }
    mostracomp(f);
    fclose(f);
}

int main(void)
{
    /* Test 1 – signed -16 777 216 (0xFF000000) */
    struct A a[1] = { { -16777216 } };
    run_test("Test 1", 1, a, "i");

    /* Test 2 – unsigned 0 */
    struct B b[1] = { { 0u } };
    run_test("Test 2", 1, b, "u");

    /* Test 3 – 63‑char string */
    struct C c[1];
    memset(c[0].s, 'X', 63);
    c[0].s[63] = '\0';
    run_test("Test 3", 1, c, "s64");

    /* Test 4 – mix -1 & 258 */
    struct D d[1] = { { -1, 258u } };
    run_test("Test 4", 1, d, "iu");

    /* Test 5 – three structs, newline spacing */
    struct E e[3] = { {1,"a"}, {2,"b"}, {3,"c"} };
    run_test("Test 5", 3, e, "is02");

    return 0;
}
