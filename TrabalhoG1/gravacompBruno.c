/* Trabalho 1 — INF1018   Armazenamento compactado
   Implementa: gravacomp() e mostracomp()

   - Bruno Tardin Fernandes 2411072 3WA
   - Douglas Gomes de Oliveira 1721586 3WA
*/
/******************************************************************************
 *  gravacomp.c                                                               *
 *  ------------------------------------------------------------------------ *
 *  Implementa as duas funções declaradas em gravacomp.h                     *
 *                                                                            *
 *      int  gravacomp (int nStructs, void *valores, char *descr, FILE *arq); *
 *      void mostracomp(FILE *arq);                                           *
 *                                                                            *
 *  Formato binario (resumo):                                                *
 *      • 1º byte  -> quantidade de structs (0‑255)                            *
 *      • Para cada campo grava‑se 1 byte de cabeçalho + dados compactados.   *
 *                                                                            *
 *  Cabeçalho de campo (1 byte):                                              *
 *      bit 7  = cont  (1 se último campo da struct)                          *
 *      bit 6  = 1 -> string   | 0 → inteiro                                   *
 *      bit 5  = (só para inteiros) 0 → unsigned | 1 -> signed                 *
 *      bits 4‑0                                                                *
 *          • string : tamanho gravado (0‑63)                                 *
 *          • inteiro: n de bytes (1‑4) em que o valor foi codificado        *
 *                                                                            *
 *  Inteiros são gravados em big‑endian no número mínimo de bytes:            *
 *      unsigned -> elimina zeros à esquerda; signed -> elimina 0xFF de sinal.  *
 *  Strings gravam de 0 a 63 bytes (sem ‘\0’).                                *
 *                                                                            *
 *  O código assume que os parâmetros recebidos são válidos.                  *
 *  Retorna ‑1 apenas se ocorrer falha de E/S (fputc/fwrite/fgetc/fread).     *
 ******************************************************************************/

#include "gravacomp.h"          /* protótipos gravacomp / mostracomp          */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 * 1.  Estruturas auxiliares
 * ==========================================================================*/

/* Identifica o tipo lógico de cada campo do descritor */
typedef enum { FK_STR,           /* campo string  (char[N])  */
               FK_UINT,          /* inteiro sem sinal        */
               FK_SINT } FieldKind; /* inteiro com sinal   */

/* Metadados gerados a partir do descritor. */
typedef struct {
    FieldKind kind;     /* string / unsigned / signed                      */
    uint16_t  cap;      /* capacidade se string (1‑64)                     */
    size_t    off;      /* deslocamento dentro da struct em bytes          */
    size_t    size;     /* tamanho em RAM (1 ou 4)                         */
    uint8_t   last;     /* 1 ⇒ último campo da struct (bit 7 do cabeçalho) */
} FieldMeta;

/* ===========================================================================
 * 2.  Utilitário – número mínimo de bytes para armazenar um inteiro
 * ==========================================================================*/
static size_t min_bytes(uint32_t x, int is_signed)
{
    if (!is_signed) {                       /* ---------- unsigned ---------- */
        if (x <= 0xFF)       return 1;
        if (x <= 0xFFFF)     return 2;
        if (x <= 0xFFFFFF)   return 3;
        return 4;
    }
    /* ------------------------------ signed ------------------------------ */
    int32_t s = (int32_t)x;
    if (s >= -128       && s <= 127)        return 1;
    if (s >= -32768     && s <= 32767)      return 2;
    if (s >= -8388608   && s <= 8388607)    return 3;
    return 4;
}

/* ===========================================================================
 * 3.  Parser do descritor  ->  vetor FieldMeta  +  sizeof(struct)
 * ==========================================================================*/
static size_t
parse_descr(const char *d, FieldMeta **meta_out, size_t *sz_out)
{
    size_t n_max = strlen(d);               /* caracteres do descritor        */
    FieldMeta *v = calloc(n_max, sizeof *v);

    size_t off = 0;                         /* deslocamento corrente          */
    size_t max_align = 1;                   /* alinhamento (1 ou 4)           */
    size_t f = 0;                           /* numero real de campos              */

    for (size_t k = 0; d[k]; ++k, ++f) {
        FieldMeta *m = &v[f];
        char c = d[k];

        if (c == 's') {                     /* ------------------ STRING ---- */
            m->kind = FK_STR;
            m->size = 1;
            /* capacidade = dígitos após 's' (01‑64)                         */
            m->cap  = (d[k+1]-'0')*10 + (d[k+2]-'0');
            k += 2;                         /* pula os dois dígitos           */
        } else {                            /* ------------------ INTEIRO --- */
            m->kind = (c == 'u') ? FK_UINT : FK_SINT;
            m->size = 4;
            max_align = 4;                  /* struct precisa alinhamento 4   */
        }

        /* alinhar offset ao tamanho do campo                               */
        off = (off + (m->size - 1)) & ~(m->size - 1);
        m->off = off;

        /* avança offset: se string usa cap bytes, se int usa 4 bytes       */
        off += (m->kind == FK_STR) ? m->cap : m->size;
    }

    if (f) v[f-1].last = 1;                 /* marca último campo real        */

    /* padding final da struct (mesma regra do compilador)                  */
    off = (off + (max_align - 1)) & ~(max_align - 1);

    *meta_out = v;
    *sz_out   = off;
    return f;                               /* retorna numero de campos           */
}

/* ===========================================================================
 * 4.  Função gravacomp  – grava o arquivo binário compacto
 * ==========================================================================*/
int gravacomp(int nstructs, void *valores, char *descr, FILE *arq)
{
    /* ---- 4.1  Pré‑processa descritor ---- */
    FieldMeta *meta = NULL;
    size_t struct_sz = 0;
    size_t n_fields  = parse_descr(descr, &meta, &struct_sz);

    /* ---- 4.2  Cabeçalho global (quantidade de structs) ---- */
    if (fputc(nstructs, arq) == EOF) { free(meta); return -1; }

    uint8_t *base = (uint8_t*)valores;      /* ponteiro para o vetor          */

    /* ---- 4.3  Loop por struct ---- */
    for (int s = 0; s < nstructs; ++s) {
        uint8_t *p = base + s * struct_sz;  /* início da struct s             */

        /* ---- 4.4  Loop por campo ---- */
        for (size_t i = 0; i < n_fields; ++i) {
            FieldMeta *m = &meta[i];

            if (m->kind == FK_STR) {        /* ---------- STRING ---------- */
                const char *str = (const char*)(p + m->off);
                /* grava no máx. cap‑1 bytes, nunca ‘\0’                    */
                size_t len = strnlen(str, m->cap - 1);

                /* bit7=cont, bit6=1(string), len em bits5‑0                */
                uint8_t hdr = (m->last << 7) | (1 << 6) | (uint8_t)len;
                if (fputc(hdr, arq) == EOF) goto io_err;
                if (fwrite(str, 1, len, arq) != len) goto io_err;
            }
            else {                          /* ---------- INT / UINT ------ */
                uint32_t raw;
                memcpy(&raw, p + m->off, 4);

                int is_signed = (m->kind == FK_SINT);
                size_t nb = min_bytes(raw, is_signed);

                /* Cabeçalho: bit7 cont | bit6=0 | bit5 signed | len(0‑31)  */
                uint8_t hdr = (m->last << 7) |
                              (is_signed << 5) |
                              (uint8_t)nb;
                if (fputc(hdr, arq) == EOF) goto io_err;

                /* grava bytes em ordem big‑endian                          */
                for (int b = nb - 1; b >= 0; --b)
                    if (fputc((raw >> (8*b)) & 0xFF, arq) == EOF) goto io_err;
            }
        }
    }
    free(meta);
    return 0;                               /* sucesso */

io_err:
    free(meta);
    return -1;                              /* falha de E/S */
}

/* ===========================================================================
 * 5.  Função mostracomp  – le o arquivo e imprime em formato humano
 * ==========================================================================*/
void mostracomp(FILE *arq)
{
    int n = fgetc(arq);                     /* numero de structs gravado          */
    if (n == EOF) return;

    printf("Estruturas: %d\n\n", n);

    for (int s = 0; s < n; ++s) {
        while (1) {
            int h = fgetc(arq);
            if (h == EOF) return;           /* fim inesperado                 */
            uint8_t hd = (uint8_t)h;

            int last   = hd >> 7;           /* bit 7  */
            int is_str = (hd >> 6) & 1;     /* bit 6  */

            if (is_str) {                   /* ----------- STRING ----------- */
                int len = hd & 0x3F;        /* bits5‑0 = tamanho (0‑63)       */
                char buf[64] = {0};
                fread(buf, 1, len, arq);
                printf("(str) %s\n", buf);
            }
            else {                          /* ----------- INTEIRO ---------- */
                int signed_flag = (hd >> 5) & 1;   /* bit 5 */
                int len = hd & 0x1F;              /* bits4‑0 = numero bytes (1‑4) */

                uint32_t val = 0;
                /* Reconstrói big‑endian                                     */
                for (int i = 0; i < len; ++i) {
                    int byte = fgetc(arq);
                    if (byte == EOF) return;
                    val = (val << 8) | (uint8_t)byte;
                }

                if (!signed_flag) {               /* unsigned */
                    printf("(uns) %u (%08x)\n", val, val);
                } else {                          /* signed   */
                    /* sign‑extend se len < 4                                   */
                    int shift = (4 - len) * 8;
                    int32_t sv = (int32_t)(val << shift) >> shift;
                    printf("(int) %d (%08x)\n", sv, sv);
                }
            }

            if (last) { putchar('\n'); break; }   /* termina struct            */
        }
    }
}
