/*
 * peqcomp.c – Gerador de código x86‑64 para a linguagem SBas
 *
 * Luan Carlos Almada Braga 2411776 Turma 3WA
 * Bruno Tardin Fernandes 2411072 Turma 3WA
 *
 * A função principal deste arquivo, @c peqcomp, recebe um descritor de arquivo
 * contendo uma função escrita em SBas e traduz cada linha para código de
 * máquina, conforme a convenção System V AMD64. O código é emitido num vetor
 * fornecido pelo chamador e um ponteiro para a função JITada é devolvido.
 *
 *  ▸ Variáveis locais  (v1..v5) → armazenadas no RA em offsets –4, –8, …
 *  ▸ Parâmetros        (p1..p3) → registradores edi, esi, edx
 *  ▸ Retorno (ret)              → registrador eax
 *  ▸ Desvios (iflez)            → jle rel32 com back‑patch posterior
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "peqcomp.h"

/** Número máximo de linhas em um arquivo SBas. */
#define MAX_LINES 30

/* -------------------------------------------------------------------------- */
/* Tipos auxiliares                                                           */
/* -------------------------------------------------------------------------- */

/** Tipos de comandos reconhecidos. */
typedef enum {
    CMD_ASSIGN,  ///< vX : varpc
    CMD_EXPR,    ///< vX = varc op varc
    CMD_IF,      ///< iflez vX n
    CMD_RET      ///< ret varc
} cmd_t;

/** Origem/destino de operandos. */
typedef enum {
    OP_LOCAL,   ///< variável local (v1..v5)
    OP_PARAM,   ///< parâmetro (p1..p3)
    OP_CONST    ///< constante literal ($k)
} op_t;

/** Estrutura interna que representa uma linha já analisada. */
typedef struct {
    cmd_t type;   ///< tipo da instrução
    int   var;    ///< destino (assign/expr) ou var testada (if)

    /* --- CMD_ASSIGN ------------------------------------------------------- */
    op_t  ap_type;
    int   ap_val;

    /* --- CMD_EXPR --------------------------------------------------------- */
    op_t  e1_type; int e1_val;   ///< primeiro operando
    char  e_op;                  ///< '+', '-' ou '*'
    op_t  e2_type; int e2_val;   ///< segundo operando

    /* --- CMD_IF ----------------------------------------------------------- */
    int   if_target;             ///< linha‑alvo (1‑based) para o desvio

    /* --- CMD_RET ---------------------------------------------------------- */
    op_t  r_type; int r_val;     ///< valor de retorno
} instr_t;

/** Marca o local onde um salto ainda precisará ser corrigido. */
typedef struct {
    int offset;   ///< posição do rel32 a ser escrito
    int target;   ///< número da linha de destino
} patch_t;

/* -------------------------------------------------------------------------- */
/* Macros utilitários                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief   Carrega um valor (var/local/const) em %eax.
 * @param   type  Tipo do operando (OP_LOCAL / OP_PARAM / OP_CONST).
 * @param   val   Índice da var/param ou valor imediato.
 *
 * Gera as instruções adequadas e avança o cursor @c cur.
 */
#define LOAD_EAX(type, val)                                                       \
    do {                                                                          \
        if ((type) == OP_LOCAL) { /* movl disp(%rbp),%eax */                      \
            p[cur++] = 0x8B; p[cur++] = 0x45;                                     \
            p[cur++] = (unsigned char)(-4 * (val));                               \
        } else if ((type) == OP_PARAM) {                                          \
            /* movl <reg>, %eax  — p1→edi, p2→esi, p3→edx */                      \
            if ((val) == 1) { p[cur++] = 0x8B; p[cur++] = 0xC7; } /* edi */       \
            else if ((val) == 2) { p[cur++] = 0x8B; p[cur++] = 0xC6; } /* esi */  \
            else              { p[cur++] = 0x8B; p[cur++] = 0xC2; } /* edx */     \
        } else { /* constante */                                                  \
            p[cur++] = 0xB8; /* mov imm32, %eax */                                \
            for (int _b = 0; _b < 4; _b++)                                        \
                p[cur++] = (unsigned char)(((int)(val) >> (8 * _b)) & 0xFF);      \
        }                                                                         \
    } while (0)

// A função LOAD_EAX é usada para carregar o valor de uma constante,
// variável local ou parâmetro diretamente em %eax, para uso posterior
// em operações aritméticas ou retorno.

// A struct patch_t armazena os dados necessários para realizar o "back-patch"
// de instruções iflez: quando a linha é processada, o destino ainda não é conhecido,
// então um placeholder é emitido e corrigido após o código ser totalmente gerado.

// Durante a segunda etapa, label_pos[i+1] guarda o endereço (em bytes) onde
// começa o código gerado para a i-ésima linha SBas (considerando 1-based),
// o que permite calcular corretamente deslocamentos relativos em saltos.

// O código de máquina gerado segue as convenções da ABI System V AMD64:
// - parâmetros p1, p2, p3 em edi, esi, edx (em 32 bits)
// - variáveis locais são armazenadas na pilha (stack frame)
// - valores de retorno são entregues em eax
// - epílogo da função sempre usa leave + ret para restaurar o stack frame

// Cada linha SBas é lida, interpretada, e convertida imediatamente para instruções
// x86-64 binárias, emitidas diretamente no vetor 'codigo'. Ao final, se necessário,
// os saltos são corrigidos e a função gerada pode ser chamada diretamente via ponteiro.

/* -------------------------------------------------------------------------- */
/* peqcomp                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief   Compila uma função SBas em tempo de execução.
 *
 * @param f      Arquivo texto já aberto apenas para leitura.
 * @param codigo Vetor‑destino onde o código de máquina será emitido.
 *
 * @return Ponteiro para a função gerada (@c funcp).
 */
funcp peqcomp(FILE *f, unsigned char codigo[]) {
    /* 1. Análise sintática -------------------------------------------------- */
    instr_t instrs[MAX_LINES];       // Vetor onde armazenamos as instruções interpretadas
    int     n_lines = 0;
    char    line[256];

    // Loop de leitura: analisa cada linha do arquivo e converte para estrutura interna
    while (fgets(line, sizeof(line), f) && n_lines < MAX_LINES) {
        char tok1[64], tok2[64];
        int  v, n_target;
        instr_t *in = &instrs[n_lines];

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        /* ----- iflez: desvio condicional ----- */
        if (sscanf(line, "iflez %s %d", tok1, &n_target) == 2) {
            in->type      = CMD_IF;
            in->var       = tok1[1] - '0';
            in->if_target = n_target;
        }
        /* ----- ret: retorno ----- */
        else if (sscanf(line, "ret %s", tok1) == 1) {
            in->type = CMD_RET;
            if (tok1[0] == 'v') { in->r_type = OP_LOCAL;  in->r_val = tok1[1] - '0'; }
            else if (tok1[0] == 'p') { in->r_type = OP_PARAM; in->r_val = tok1[1] - '0'; }
            else { in->r_type = OP_CONST; in->r_val = atoi(tok1 + 1); }
        }
        /* ----- atribuição: vX : varpc ----- */
        else if (strchr(line, ':') && !strchr(line, '=')) {
            if (sscanf(line, "v%d : %s", &v, tok1) == 2) {
                in->type = CMD_ASSIGN; in->var = v;
                if (tok1[0] == 'v') { in->ap_type = OP_LOCAL;  in->ap_val = tok1[1] - '0'; }
                else if (tok1[0] == 'p') { in->ap_type = OP_PARAM; in->ap_val = tok1[1] - '0'; }
                else { in->ap_type = OP_CONST; in->ap_val = atoi(tok1 + 1); }
            }
        }
        /* ----- expressão aritmética: vX = a op b ----- */
        else if (strchr(line, '=') &&
                 sscanf(line, "v%d = %s %c %s", &v, tok1, &in->e_op, tok2) == 4) {
            in->type = CMD_EXPR; in->var = v;
            if (tok1[0] == 'v') { in->e1_type = OP_LOCAL; in->e1_val = tok1[1] - '0'; }
            else if (tok1[0] == 'p') { in->e1_type = OP_PARAM; in->e1_val = tok1[1] - '0'; }
            else { in->e1_type = OP_CONST; in->e1_val = atoi(tok1 + 1); }

            if (tok2[0] == 'v') { in->e2_type = OP_LOCAL; in->e2_val = tok2[1] - '0'; }
            else if (tok2[0] == 'p') { in->e2_type = OP_PARAM; in->e2_val = tok2[1] - '0'; }
            else { in->e2_type = OP_CONST; in->e2_val = atoi(tok2 + 1); }
        } else {
            continue;
        }
        n_lines++;
    }

    /* 2. Geração de código -------------------------------------------------- */
    int   label_pos[MAX_LINES + 1] = {0}; // Endereço inicial de cada linha SBas (1-based)
    patch_t patches[MAX_LINES];          // Saltos pendentes (jle)
    int   n_patches = 0;
    unsigned char *p = codigo;
    int   cur = 0;

    // Prólogo da função: prepara o stack frame e reserva espaço para 5 variáveis locais
    p[cur++] = 0x55;
    p[cur++] = 0x48; p[cur++] = 0x89; p[cur++] = 0xE5;
    p[cur++] = 0x48; p[cur++] = 0x83; p[cur++] = 0xEC; p[cur++] = 0x20;

    // Loop principal de tradução linha por linha
    for (int i = 0; i < n_lines; i++) {
        instr_t *in = &instrs[i];
        label_pos[i + 1] = cur;

        switch (in->type) {
        case CMD_ASSIGN:
            LOAD_EAX(in->ap_type, in->ap_val);
            p[cur++] = 0x89; p[cur++] = 0x45;
            p[cur++] = (unsigned char)(-4 * in->var);
            break;

        case CMD_EXPR:
            LOAD_EAX(in->e1_type, in->e1_val);
            if (in->e2_type == OP_LOCAL) {
                p[cur++] = 0x8B; p[cur++] = 0x4D;
                p[cur++] = (unsigned char)(-4 * in->e2_val);
            } else if (in->e2_type == OP_PARAM) {
                if (in->e2_val == 1) { p[cur++] = 0x8B; p[cur++] = 0xCF; }
                else if (in->e2_val == 2) { p[cur++] = 0x8B; p[cur++] = 0xCE; }
                else                     { p[cur++] = 0x8B; p[cur++] = 0xCA; }
            } else {
                p[cur++] = 0xB9;
                for (int b = 0; b < 4; b++)
                    p[cur++] = (unsigned char)((in->e2_val >> (8 * b)) & 0xFF);
            }
            if (in->e_op == '+') { p[cur++] = 0x01; p[cur++] = 0xC8; }
            else if (in->e_op == '-') { p[cur++] = 0x29; p[cur++] = 0xC8; }
            else { p[cur++] = 0x0F; p[cur++] = 0xAF; p[cur++] = 0xC1; }
            p[cur++] = 0x89; p[cur++] = 0x45;
            p[cur++] = (unsigned char)(-4 * in->var);
            break;

        case CMD_IF:
            LOAD_EAX(OP_LOCAL, in->var);
            p[cur++] = 0x83; p[cur++] = 0xF8; p[cur++] = 0x00;
            p[cur++] = 0x0F; p[cur++] = 0x8E;
            patches[n_patches].offset = cur;
            patches[n_patches].target = in->if_target;
            n_patches++;
            for (int z = 0; z < 4; z++) p[cur++] = 0x00;
            break;

        case CMD_RET:
            LOAD_EAX(in->r_type, in->r_val);
            p[cur++] = 0xC9;
            p[cur++] = 0xC3;
            break;
        }
    }

    // Adiciona epílogo padrão se último comando não for ret
    if (instrs[n_lines - 1].type != CMD_RET) {
        p[cur++] = 0xC9;
        p[cur++] = 0xC3;
    }

    /* 3. Correção dos desvios pendentes (back‑patch) ----------------------- */
    for (int i = 0; i < n_patches; i++) {
        int off   = patches[i].offset;
        int dest  = label_pos[patches[i].target];
        int rel32 = dest - (off + 4);
        for (int b = 0; b < 4; b++)
            codigo[off + b] = (unsigned char)((rel32 >> (8 * b)) & 0xFF);
    }

    return (funcp)codigo;
}
