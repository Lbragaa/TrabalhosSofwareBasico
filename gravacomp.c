/* Trabalho 1 — INF1018   Armazenamento compactado
   Implementa: gravacomp() e mostracomp()

   • Luan Carlos Almada Braga 2411776 3WA
   • Diogo  Matrícula  3WA
*/

/* --- Bibliotecas padrão usadas ---------------------------------
   <stdint.h>  tipos inteiros de tamanho fixo (ex.: uint32_t)
   <stdlib.h>  funções gerais (malloc, exit, etc.) – aqui usamos só exit
   <string.h>  manipulação de strings/buffers (strnlen, memcpy, memset)
   <ctype.h>   macros para testar caracteres (isdigit)
*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gravacomp.h"

/* ---------- Tipos auxiliares -----------------------------------
   Precisamos descrever cada campo da struct (tipo, tamanho, etc.)
------------------------------------------------------------------*/

/* ---------- tipos auxiliares ---------------------------------------- */


/* Enum que identifica o tipo lógico do campo */
typedef enum { FT_STR,        /* string  (char[])        */
               FT_UINT,       /* unsigned int            */
               FT_INT         /* signed int              */
} FieldType;

/* Informações completas de um campo, calculadas a partir
   do descritor "iis03us10" etc. */
typedef struct {
    FieldType  type;     /* tipo lógico (enum acima)                 */
    uint8_t    sizeMax;  /* para strings: capacidade do vetor        */
    uint16_t   offset;   /* deslocamento (em bytes) dentro da struct, tendo considerado padding */
    uint8_t    isLast;   /* 1 se é o último campo da struct          */
} FieldInfo;

/* Limite máximo de campos por struct que aceitaremos.
   Valor arbitrário alto só para evitar estouro de array local. */
#define MAX_FIELDS 128

/* ---------- Protótipos de funções internas --------------------- */
/* Converte descritor em array de FieldInfo e calcula tamanho da struct */
static int     parse_descriptor(const char *desc,
                                FieldInfo info[MAX_FIELDS],
                                int *nFields,
                                size_t *structSize);
/* Calcula quantos bytes são necessários p/ representar um inteiro */
static size_t  int_min_bytes(uint32_t v, int isSigned);
/* Grava 'len' bytes do inteiro v em big‑endian no buffer buf      */
static void    int_to_be(uint32_t v, unsigned char *buf, size_t len);

/* ---------- GRAVACOMP ---------------------------------------------- */

/* --------------------------------------------------------------------
   gravacomp
   Grava em arquivo binário, de forma compactada, um array de structs.

   Parâmetros:
     nstructs  – quantidade de elementos no vetor
     valores   – ponteiro para o início do vetor
     campos    – string descritor (ex.: "iis03us10")
     arq       – FILE* já aberto em "wb"

   REGRAS:
   • Assumimos que todos os parâmetros são válidos (enunciado).
   • Só retornamos -1 se ocorrer erro de I/O (fputc, fwrite).
   ------------------------------------------------------------------*/
int gravacomp(int nstructs, void *valores, char *campos, FILE *arq)
{
    /* 1) Converte o descritor textual em informações de cada campo
          (offset, tamanho, flag de último).                           */
    FieldInfo fi[MAX_FIELDS];  /* vetor estático para até 128 campos   */
    int   nF;                  /* nº de campos encontrados             */
    size_t structSz;           /* tamanho total da struct (com padding)*/
    parse_descriptor(campos, fi, &nF, &structSz);
    /* -> Não checamos retorno porque o enunciado garante validade.    */

    /* 2) Primeiro byte do arquivo = quantidade de structs             */
    if (fputc((unsigned char)nstructs, arq) == EOF)
        return -1;  /* falha de gravação */

    /* 3) Percorremos o vetor: “base” aponta para o 1º elemento        */
    unsigned char *base = (unsigned char *)valores;

    for (int s = 0; s < nstructs; ++s) {
        /* ponteiro para o início da struct atual                      */
        unsigned char *p = base + s * structSz;

        /* 3a) Percorrer cada campo da struct                          */
        for (int f = 0; f < nF; ++f) {
            FieldInfo *fld = &fi[f];

            /* Cabeçalho inicia com bit 7 = 1 se é o último campo      */
            uint8_t header = fld->isLast ? 0x80 : 0x00;

            /* ---------- Campo: STRING ------------------------------ */
            if (fld->type == FT_STR) {
                char *str  = (char *)(p + fld->offset);            /* valor */
                size_t len = strnlen(str, fld->sizeMax - 1);       /* <=63  */
                if (len > 63) len = 63;                            /* limite*/

                header |= 0x40;            /* bit 6 = 1 → tipo string   */
                header |= (uint8_t)len;    /* bits 5‑0 = tamanho útil   */

                if (fputc(header, arq) == EOF)                return -1;
                if (fwrite(str, 1, len, arq) != len)          return -1;
            }

            /* ---------- Campo: INTEIRO (signed ou unsigned) -------- */
            else {
                uint32_t v;                                        /* valor */
                /* memcpy evita problema de strict‑aliasing           */
                memcpy(&v, p + fld->offset, sizeof(uint32_t));

                int    isSigned = (fld->type == FT_INT);
                size_t len      = int_min_bytes(v, isSigned);  /* 1‑4 bytes*/

                /* bits 6‑5 = 01 se signed, 00 se unsigned             */
                header |= (isSigned ? 0x20 : 0x00);
                /* bits 4‑0 = nº de bytes gravados                     */
                header |= (uint8_t)len;

                unsigned char buf[4];
                int_to_be(v, buf, len);    /* converte p/ big‑endian   */

                if (fputc(header, arq) == EOF)                return -1;
                if (fwrite(buf, 1, len, arq) != len)          return -1;
            }
        } /* fim for campos */
    }     /* fim for structs */

    return 0;   /* sucesso: nenhum erro de E/S */
}


/* --------------------------------------------------------------------
   mostracomp
   Lê um arquivo gerado por gravacomp e imprime, no formato especificado,
   todos os valores armazenados.

   Regras de impressão (exemplo do enunciado):
     Estruturas: N

     (int)  -1 (ffffffff)
     (str)  abc
     (uns)  258 (00000102)

   A função NÃO fecha o arquivo.                                         */
void mostracomp(FILE *arq)
{
    /* ----- 1) Lê o 1º byte: quantidade de structs ------------------ */
    int c = fgetc(arq);
    if (c == EOF) {                       /* arquivo vazio ou erro de I/O */
        fputs("Arquivo vazio.\n", stderr);
        return;
    }

    printf("Estruturas: %d\n\n", c);
    int remaining = c;                    /* quantas structs faltam exibir */

    /* ----- 2) Loop sobre cada struct -------------------------------- */
    while (remaining--) {

        int lastSeen = 0;                 /* flag: já chegamos ao último
                                             campo desta struct?         */

        /* Loop interno lê campo a campo até encontrar bit cont = 1     */
        while (!lastSeen) {

            int h = fgetc(arq);           /* lê cabeçalho de 1 byte       */
            if (h == EOF) {               /* arquivo truncado?           */
                fputs("Formato incorreto.\n", stderr);
                return;
            }

            lastSeen = (h & 0x80) != 0;   /* bit 7: último campo? Colocando a flag de acordo  */

            /* ---------- Campo STRING -------------------------------- */
            if (h & 0x40) {               /* bit 6 = 1 -> string          */
                size_t len = h & 0x3F;    /* bits 5‑0: comprimento (1‑63)*/
                char s[65] = {0};         /* +1 para '\0'                */

                if (len > 64) len = 64;   /* segurança (arquivo corromp.)*/

                if (fread(s, 1, len, arq) != len) return;
                s[len] = '\0';            /* garante término de string   */

                printf("(str) %s\n", s);
            }

            /* ---------- Campo INTEIRO / UNS ------------------------- */
            else {
                int    isSigned = (h & 0x60) == 0x20; /* 01? signed */
                size_t len      = h & 0x1F;           /* 1‑4 bytes  */

                if (len < 1 || len > 4) {            
                    fputs("len inválido\n", stderr);
                    return;
                }

                /* Preenche buf alinhado à direita;
                   Ex.: len=2 → bytes vão para buf[2] e buf[3]          */
                unsigned char buf[4] = {0};
                if (fread(buf + (4 - len), 1, len, arq) != len) return;

                /* Converte buffer big‑endian para inteiro de 32 bits   */
                uint32_t val = 0;
                for (size_t i = 0; i < 4; i++){  /* percorre buf [0..3]    */
                    val = (val << 8) | buf[i];   /* shift + OR incremental */
                }

                if (isSigned) {
                    /* Ajusta sinal: alinha para 32 bits mantendo valor */
                    int32_t sval = (int32_t)(val << (8 * (4 - len)));
                    sval >>= (8 * (4 - len));
                    printf("(int) %d (%08x)\n", sval, (uint32_t)sval);
                } else {
                    printf("(uns) %u (%08x)\n", val, val);
                }
            }
        } /* fim do while campos */

        /* Linhas em branco APENAS entre structs, não depois da última  */
        if (remaining) putchar('\n');
    }
}


/* --------------------------------------------------------------------
   parse_descriptor
   Converte a string “campos” (ex.: "iis03us10") em um vetor info[]
   contendo, para cada campo:
     • tipo         (FT_STR / FT_UINT / FT_INT)
     • sizeMax      (capacidade da string se for FT_STR)
     • offset       (posição real dentro da struct)
     • isLast       (1 para o último campo)

   OBS.: O enunciado garante que o descritor é sempre válido, portanto
   não testamos erros de sintaxe, nem limites de tamanho; apenas
   construímos a tabela de layout.                                            */
static int parse_descriptor(const char *desc,
                            FieldInfo info[MAX_FIELDS],
                            int *nF,
                            size_t *structSize)
{
    int    idx = 0;           /* índice em info[]                        */
    size_t off = 0;           /* deslocamento atual dentro da struct     */
    size_t maxAlign = 1;      /* maior alinhamento visto (1 ou 4)        */

    /* Percorre a string até o ‘\0’; assumimos que cabe em MAX_FIELDS   */
    while (*desc && idx < MAX_FIELDS) {

        FieldInfo f = {0};        /* zera todos os campos                */
        char ch = *desc++;        /* caractere atual e avança ponteiro   */

        if (ch == 's') {          /* ---- campo string ----------------- */
            /* Existem dois dígitos logo após 's'                        */
            f.type = FT_STR;
            f.sizeMax = (desc[0]-'0')*10 + (desc[1]-'0'); /* 01‑64       */
            desc += 2;            /* pula os dígitos                     */
        }
        else if (ch == 'u') {     /* ---- unsigned int ----------------- */
            f.type = FT_UINT;
        }
        else {                    /* ---- signed int (única opção resto)*/
            /* se não é 's' nem 'u', só pode ser 'i'                     */
            f.type = FT_INT;
        }

        /* Alinhamento e tamanho em memória                              */
        size_t align = (f.type == FT_STR) ? 1 : 4;
        size_t size  = (f.type == FT_STR) ? f.sizeMax : 4;

        /* Ajusta deslocamento para o próximo múltiplo de ‘align’        */
        if (off % align) off += align - (off % align);

        f.offset = (uint16_t)off; /* salva deslocamento                  */
        off += size;              /* avança para próximo campo           */

        if (align > maxAlign) maxAlign = align;
        info[idx++] = f;          /* guarda descrição no vetor           */
    }

    /* Marca o último campo (há pelo menos 1, garantido pelo enunciado)  */
    info[idx-1].isLast = 1;

    /* Ajusta tamanho total da struct para múltiplo de maior alinhamento */
    if (off % maxAlign) off += maxAlign - (off % maxAlign);

    *structSize = off;
    *nF         = idx;
    return 0;                     
}



/* --------------------------------------------------------------------
   int_min_bytes
   Calcula quantos bytes (1‒4) são necessários para representar um
   inteiro de 32 bits, obedecendo às regras do formato compacto:

     • Unsigned  : remover zeros à esquerda.
     • Signed    : remover bytes que repetem apenas o bit de sinal
                   (0x00 para positivos, 0xFF para negativos).

   Parâmetros:
     v         – valor original (interpretado sempre como 32 bits)
     isSigned  – 1 se o campo é signed, 0 se unsigned

   Retorna:
     1, 2, 3 ou 4  (número de bytes que *serão gravados* no arquivo)
--------------------------------------------------------------------*/
static size_t int_min_bytes(uint32_t v, int isSigned)
{
    /* -------------------------- Caso UNSIGNED ---------------------- */
    if (!isSigned) {                /* unsigned: descarta zeros à esquerda */
        size_t len = 4;             /* começamos assumindo 4 bytes          */
        /* Enquanto o byte mais alto for 0 e ainda restarem >1 bytes,
           reduzimos o comprimento.                                        */
        while (len > 1 &&
               ((v >> ((len-1)*8)) & 0xFF) == 0)
            --len;
        return len;                 /* 1‒4 bytes conforme valor            */
    }

    /* -------------------------- Caso SIGNED ------------------------ */
    else {
        int32_t s = (int32_t)v;     /* interpreta v como signed            */
        size_t len = 4;

        /* Loop de trás para frente: testa se o byte mais significativo
           (msb) é redundante — isto é, igual ao bit de sinal do próximo
           byte (msb2).                                                   */
        while (len > 1) {
            uint8_t msb  = (s >> ((len-1)*8)) & 0xFF;  /* byte atual   */
            uint8_t msb2 = (s >> ((len-2)*8)) & 0xFF;  /* byte seguinte*/

            int redundante_pos = (msb == 0x00) && ((msb2 & 0x80) == 0);
            int redundante_neg = (msb == 0xFF) && ((msb2 & 0x80) != 0);

            if (redundante_pos || redundante_neg)
                --len;              /* descarta este byte e continua      */
            else
                break;              /* encontrou primeiro byte necessário */
        }
        return len;                 /* 1‒4 bytes necessários              */
    }
}


/* --------------------------------------------------------------------
   int_to_be
   Copia para o buffer 'buf' os 'len' bytes mais significativos
   do inteiro v, em **big‑endian** (byte mais significativo primeiro).

   Parâmetros:
     v    – valor de 32 bits já calculado (signed ou unsigned)
     buf  – ponteiro para um vetor de pelo menos 4 bytes
     len  – 1‒4  (número de bytes que realmente queremos gravar)

   Ex.:  v = 0x0001_02FF   len = 2   → buf[0] = 0x01, buf[1] = 0x02
--------------------------------------------------------------------*/
static void int_to_be(uint32_t v, unsigned char *buf, size_t len)
/* len = 1‑4, buf é saída */
{
    /* i = 0 grava o byte mais significativo;
       i = len‑1 grava o byte menos significativo que ainda interessa.  */
    for (size_t i = 0; i < len; ++i)
        buf[i] = (unsigned char)(v >> (8 * (len - i - 1)));
}

