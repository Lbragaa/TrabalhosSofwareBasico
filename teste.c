#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "gravacomp.h"

#define TEST_FILE "dados.bin"

static void run_test(const char *label,
                     int n, void *vec,
                     const char *desc)
{
    printf("=== %s ===\n", label);
    FILE *f = fopen(TEST_FILE, "wb");
    if (!f) { perror("fopen"); return; }
    if (gravacomp(n, vec, (char*)desc, f) != 0) {
        printf("gravacomp falhou!\n");
        fclose(f);
        return;
    }
    fclose(f);

    f = fopen(TEST_FILE, "rb");
    if (!f) { perror("fopen"); return; }
    mostracomp(f);
    fclose(f);
    printf("\n");
}

int main(void)
{
    /* Teste 1: exemplo oficial do enunciado */
    struct Ex1 { int i; char s1[5]; unsigned u; } ex1[] = {
        { -1, "abc",  258   },
        {  1, "ABCD", 65535 }
    };
    run_test("Assignment example", 2, ex1, "is05u");

    /* Teste 2: int -16777216 */
    struct A { int32_t si; } a[] = { { -16777216 } };
    run_test("Test 1  int -16777216", 1, a, "i");

    /* Teste 3: unsigned 0 */
    struct B { unsigned u; } b[] = { { 0 } };
    run_test("Test 2  unsigned 0", 1, b, "u");

    /* Teste 4: 63-char string */
    struct C { char s[64]; } c[] = { { "" } };
    memset(c[0].s, 'X', 63);
    c[0].s[63] = '\0';
    run_test("Test 3  63-char string", 1, c, "s64");

    /* Teste 5: -1 & 258 */
    struct D { int32_t si; unsigned u; } d[] = { { -1, 258 } };
    run_test("Test 4  -1 & 258", 1, d, "iu");

    /* Teste 6: int+str[2] *3 */
    struct E { int32_t si; char s[2]; } e[] = {
        { 1, "a" }, { 2, "b" }, { 3, "c" }
    };
    run_test("Test 5  int+str[2] *3", 3, e, "is02");

    /* Teste 7: str[3] + max unsigned */
    struct F { char s[3]; unsigned u; } fmax[] = {
        { "&l", 0xFFFFFFFFu }
    };
    run_test("Test 6  str[3]+max unsigned", 1, fmax, "s03u");

    /* Teste 8: two ints */
    struct G { int32_t si; } g[] = {
        { 123456789 }, { -987654321 }
    };
    run_test("Test 7  two ints", 2, g, "i");

    /* Teste 9: int, str[3], unsigned */
    struct H { int32_t si; char s[3]; unsigned u; } h[] = {
        { 42, "ok", 256 }
    };
    run_test("Test 8  int str[3] unsigned", 1, h, "is03u");

    /* Teste 10: três strings */
    struct I { char s[6]; } iv[] = {
        { "a" }, { "PUC" }, { "RIO" }
    };
    run_test("Test 9  three strings", 3, iv, "s06");

    /* Teste 11: unsigned, int, str[4] */
    struct J { unsigned u; int32_t si; char s[4]; } j[] = {
        { 1000, -55, "Puc" }
    };
    run_test("Test 10 unsigned int str[4]", 1, j, "uis04");

    return 0;
}
