/* Arthur Rodrigues Alves Barbosa 2310394 3WA */
/* Enzo Faia Guerrieri de Castro 2410302 3WA */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "peqcomp.h"

#define MAX_LINHAS 100

// Função auxiliar para emitir um byte no código gerado e avançar o ponteiro
/**
 * @brief Emite 1 byte no vetor de código.
 * @param codigo Vetor onde o código de máquina está sendo armazenado.
 * @param pc Ponteiro para o índice atual de escrita no vetor.
 * @param byte Byte a ser escrito.
 */
void emit1(unsigned char *codigo, int *pc, unsigned char byte) {
    codigo[(*pc)++] = byte;
}

// Função auxiliar para emitir 4 bytes (int) em little-endian no código gerado
/**
 * @brief Emite 4 bytes (int) no vetor de código utilizando a função emit1, em ordem little-endian.
 * @param codigo Vetor onde o código será armazenado.
 * @param pc Ponteiro para o índice atual de escrita.
 * @param valor Valor inteiro de 4 bytes a ser emitido.
 */
void emit4(unsigned char *codigo, int *pc, int valor) {
    emit1(codigo, pc, valor & 0xFF);
    emit1(codigo, pc, (valor >> 8) & 0xFF);
    emit1(codigo, pc, (valor >> 16) & 0xFF);
    emit1(codigo, pc, (valor >> 24) & 0xFF);
}



// Calcula o offset da variável local na pilha (stack frame)
// As variáveis locais são v1..v5, armazenadas com 4 bytes cada
/**
 * @brief Calcula o deslocamento de uma variável local no stack frame.
 * @param var Nome da variável local (ex: "v1").
 * @return Offset negativo relativo a rbp.
 */
int offset_var(const char *var) {
    if (var[0] != 'v') return -1;
    int idx = atoi(var + 1);
    return -4 * idx; // offset negativo porque estão na stack
}

// Retorna o código do registrador para os parâmetros p1, p2, p3 segundo convenção Linux x86-64
// p1 -> edi (0xFF), p2 -> esi (0xFE), p3 -> edx (0xFA)
/**
 * @brief Retorna um identificador simbólico para os registradores dos parâmetros p1, p2, p3.
 * @param param Nome do parâmetro (ex: "p1").
 * @return Código simbólico do registrador ou -1 se inválido.
 */
int reg_param(const char *param) {
    if (param[0] != 'p') return -1;
    int idx = atoi(param + 1);
    if (idx == 1) return 0xFF; // edi
    if (idx == 2) return 0xFE; // esi
    if (idx == 3) return 0xFA; // edx
    return -1;
}

// Emite o prólogo da função (setup da pilha e base frame)
// push rbp; mov rbp, rsp; sub rsp, 32 (espaço para 8 variáveis locais * 4 bytes é 32)
/**
 * @brief Emite o prólogo padrão da função, salvando rbp e alocando espaço na stack.
 * @param codigo Vetor de código onde será emitido.
 * @param pc Ponteiro para índice atual de código.
 */
void emitir_prologo(unsigned char *codigo, int *pc) {
    emit1(codigo, pc, 0x55); // push rbp
    emit1(codigo, pc, 0x48); emit1(codigo, pc, 0x89); emit1(codigo, pc, 0xE5); // mov rbp, rsp
    emit1(codigo, pc, 0x48); emit1(codigo, pc, 0x83); emit1(codigo, pc, 0xEC); emit1(codigo, pc, 0x20); // sub rsp, 32
}


// Emite o epílogo da função (libera stack frame e retorna)
// mov rsp, rbp; pop rbp; ret
/**
 * @brief Emite o epílogo padrão da função, restaurando rbp e retornando.
 * @param codigo Vetor de código onde será emitido.
 * @param pc Ponteiro para índice atual de código.
 */
void emitir_epilogo(unsigned char *codigo, int *pc) {
    emit1(codigo, pc, 0x48); emit1(codigo, pc, 0x89); emit1(codigo, pc, 0xEC); // mov rsp, rbp
    emit1(codigo, pc, 0x5D); // pop rbp
    emit1(codigo, pc, 0xC3); // ret
}


// Emite o código para o comando 'ret' da linguagem SBas
// Pode retornar constante, variável local ou parâmetro
/**
 * @brief Emite código de retorno da função SBas.
 * @param codigo Vetor onde será emitido o código.
 * @param pc Ponteiro para posição atual.
 * @param arg Valor de retorno: constante, variável ou parâmetro.
 */
void emitir_ret(unsigned char *codigo, int *pc, const char *arg) {
    if (arg[0] == '$') {
        // Retorno constante: mov eax, valor
        emit1(codigo, pc, 0xB8);
        emit4(codigo, pc, atoi(arg + 1));
    } else if (arg[0] == 'v') {
        // Retorno variável local: mov eax, [rbp + offset]
        int offset = offset_var(arg);
        emit1(codigo, pc, 0x8B); emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset);
    } else if (arg[0] == 'p') {
        // Retorno parâmetro: mov eax, registrador correspondente
        int idx = atoi(arg + 1);
        if (idx == 1) { emit1(codigo, pc, 0x89); emit1(codigo, pc, 0xF8); } // mov eax, edi
        else if (idx == 2) { emit1(codigo, pc, 0x89); emit1(codigo, pc, 0xF0); } // mov eax, esi
        else if (idx == 3) { emit1(codigo, pc, 0x89); emit1(codigo, pc, 0xD0); } // mov eax, edx
    }
    emitir_epilogo(codigo, pc); // finaliza função
}

// Emite código para atribuição var : varpc
// dest é variável local (v1..v5)
// src pode ser constante, variável local ou parâmetro
/**
 * @brief Emite instrução de atribuição para uma variável local.
 * @param codigo Vetor de código.
 * @param pc Ponteiro para índice atual.
 * @param dest Variável local de destino.
 * @param src Fonte: constante, variável ou parâmetro.
 */
void emitir_atribuicao(unsigned char *codigo, int *pc, const char *dest, const char *src) {
    int offset_dest = offset_var(dest);
    if (src[0] == '$') {
        // mov dword ptr [rbp+offset_dest], valor
        emit1(codigo, pc, 0xC7); emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset_dest);
        emit4(codigo, pc, atoi(src + 1));
    } else if (src[0] == 'v') {
        // mov eax, [rbp+offset_src]
        int offset_src = offset_var(src);
        emit1(codigo, pc, 0x8B); emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset_src);
        // mov [rbp+offset_dest], eax
        emit1(codigo, pc, 0x89); emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset_dest);
    } else if (src[0] == 'p') {
        // mov [rbp+offset_dest], reg_param
        int reg = reg_param(src);
        if (reg == 0xFF) { emit1(codigo, pc, 0x89); emit1(codigo, pc, 0x7D); } // mov [rbp+offset_dest], edi
        else if (reg == 0xFE) { emit1(codigo, pc, 0x89); emit1(codigo, pc, 0x75); } // mov [rbp+offset_dest], esi
        else if (reg == 0xFA) { emit1(codigo, pc, 0x89); emit1(codigo, pc, 0x55); } // mov [rbp+offset_dest], edx
        emit1(codigo, pc, (unsigned char)offset_dest);
    }
}

// Emite código para expressão aritmética var = varc op varc
// var = variável local de destino
// varc = variável local, parâmetro ou constante
// op = +, -, *
// Gera código mov para carregar primeiro operando em eax, depois aplica segundo operando com op,
// finalmente armazena resultado em var
/**
 * @brief Emite código de operação aritmética (soma, subtração ou multiplicação).
 * @param codigo Vetor de código.
 * @param pc Ponteiro para índice atual.
 * @param dest Variável local destino.
 * @param op1 Primeiro operando (variável ou constante).
 * @param op Operador: "+", "-" ou "*".
 * @param op2 Segundo operando (variável, parâmetro ou constante).
 */
void emitir_expr(unsigned char *codigo, int *pc, const char *dest, const char *op1, const char *op, const char *op2) {
    // Carrega op1 em eax
    if (op1[0] == '$') {
        emit1(codigo, pc, 0xB8); emit4(codigo, pc, atoi(op1 + 1)); // mov eax, constante
    } else if (op1[0] == 'v') {
        int offset = offset_var(op1);
        emit1(codigo, pc, 0x8B); emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset); // mov eax, [rbp+offset]
    }

    // Aplica op com op2
    if (op2[0] == '$') {
        int val = atoi(op2 + 1);
        if (strcmp(op, "+") == 0) { emit1(codigo, pc, 0x05); emit4(codigo, pc, val); }   // add eax, val
        else if (strcmp(op, "-") == 0) { emit1(codigo, pc, 0x2D); emit4(codigo, pc, val); } // sub eax, val
        else if (strcmp(op, "*") == 0) { emit1(codigo, pc, 0x69); emit1(codigo, pc, 0xC0); emit4(codigo, pc, val); } // imul eax, val
    } else if (op2[0] == 'p') {
        // operando 2 é parâmetro: aplica op entre eax e registrador do parâmetro
        if (strcmp(op, "+") == 0) {
            if (strcmp(op2, "p1") == 0) { emit1(codigo, pc, 0x03); emit1(codigo, pc, 0xC7); } // add eax, edi
            else if (strcmp(op2, "p2") == 0) { emit1(codigo, pc, 0x03); emit1(codigo, pc, 0xC6); } // add eax, esi
            else if (strcmp(op2, "p3") == 0) { emit1(codigo, pc, 0x03); emit1(codigo, pc, 0xC2); } // add eax, edx
        } else if (strcmp(op, "-") == 0) {
            if (strcmp(op2, "p1") == 0) { emit1(codigo, pc, 0x2B); emit1(codigo, pc, 0xC7); } // sub eax, edi
            else if (strcmp(op2, "p2") == 0) { emit1(codigo, pc, 0x2B); emit1(codigo, pc, 0xC6); } // sub eax, esi
            else if (strcmp(op2, "p3") == 0) { emit1(codigo, pc, 0x2B); emit1(codigo, pc, 0xC2); } // sub eax, edx
        } else if (strcmp(op, "*") == 0) {
            if (strcmp(op2, "p1") == 0) { emit1(codigo, pc, 0x0F); emit1(codigo, pc, 0xAF); emit1(codigo, pc, 0xC7); } // imul eax, edi
            else if (strcmp(op2, "p2") == 0) { emit1(codigo, pc, 0x0F); emit1(codigo, pc, 0xAF); emit1(codigo, pc, 0xC6); } // imul eax, esi
            else if (strcmp(op2, "p3") == 0) { emit1(codigo, pc, 0x0F); emit1(codigo, pc, 0xAF); emit1(codigo, pc, 0xC2); } // imul eax, edx
        }
    } else {
        // op2 é variável local
        int offset = offset_var(op2);
        if (strcmp(op, "+") == 0) { emit1(codigo, pc, 0x03); }   // add eax, [rbp+offset]
        else if (strcmp(op, "-") == 0) { emit1(codigo, pc, 0x2B); } // sub eax, [rbp+offset]
        else if (strcmp(op, "*") == 0) { emit1(codigo, pc, 0x0F); emit1(codigo, pc, 0xAF); } // imul eax, [rbp+offset]
        emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset);
    }

    // Armazena resultado em dest: mov [rbp+offset_dest], eax
    int offset_dest = offset_var(dest);
    emit1(codigo, pc, 0x89); emit1(codigo, pc, 0x45); emit1(codigo, pc, (unsigned char)offset_dest);
}

// Função principal do micro-compilador
// Lê arquivo SBas, gera código máquina x86-64 na área codigo[]
// Retorna ponteiro para função gerada
/**
 * @brief Função principal do compilador: lê arquivo SBas, gera código x86-64.
 * @param f Ponteiro para arquivo de entrada (.sbas).
 * @param codigo Vetor de memória onde o código será gerado.
 * @return Ponteiro para a função compilada, do tipo funcp.
 */
funcp peqcomp(FILE *f, unsigned char codigo[]) {
    char linhas[MAX_LINHAS][100];  // armazenar linhas do código SBas
    int enderecos[MAX_LINHAS] = {0}; // endereço de cada linha (offset no código gerado)
    int saltos[MAX_LINHAS] = {0};    // posições dos saltos para consertar depois
    int num_linhas = 0;
    int pc = 0;  // contador do próximo byte no código gerado

    // Lê todas as linhas do arquivo (máximo MAX_LINHAS)
    while (fgets(linhas[num_linhas], sizeof(linhas[num_linhas]), f)) {
        num_linhas++;
    }

    emitir_prologo(codigo, &pc);  // inicia função com prólogo padrão

    for (int i = 0; i < num_linhas; i++) {
        printf("Linha %d: %s", i + 1, linhas[i]); // debug print da linha lida

        enderecos[i + 1] = pc;  // marca endereço (offset) do início da próxima linha (i+1)

        // Copia linha para temp para uso do strtok
        char temp[100];
        strcpy(temp, linhas[i]);
        char *cmd = strtok(temp, " \n");
        if (!cmd) continue;

        // Detecta tipo de comando e chama função emissora correspondente
        if (strcmp(cmd, "ret") == 0) {
            char *arg = strtok(NULL, " \n");
            emitir_ret(codigo, &pc, arg);
        } else if (strcmp(cmd, "iflez") == 0) {
            char *arg = strtok(NULL, " \n"); // variável local
            strtok(NULL, " \n");             // número da linha (ignoramos por enquanto)

            // Emite código para testar se var <= 0 e saltar condicionalmente
            int offset = offset_var(arg);
            emit1(codigo, &pc, 0x8B); emit1(codigo, &pc, 0x45); emit1(codigo, &pc, (unsigned char)offset); // mov eax, [rbp+offset]
            emit1(codigo, &pc, 0x85); emit1(codigo, &pc, 0xC0); // test eax, eax
            emit1(codigo, &pc, 0x0F); emit1(codigo, &pc, 0x8E); // jle (jump if less or equal) com 32bit offset

            saltos[i] = pc;  // guarda posição para corrigir offset depois
            emit4(codigo, &pc, 0); // espaço reservado para offset do salto
        } else if (strchr(linhas[i], ':')) {
            // Linha contém ':' -> comando de atribuição var : varpc
            char dest[10], src[10];
            if (sscanf(linhas[i], "%[^:]: %s", dest, src) == 2) {
                emitir_atribuicao(codigo, &pc, dest, src);
            }
        } else {
            // Caso contrário, expressão aritmética: var = varc op varc
            char *dest = cmd;
            strtok(NULL, " \n"); // descarta '='
            char *op1 = strtok(NULL, " \n");
            char *op = strtok(NULL, " \n");
            char *op2 = strtok(NULL, " \n");
            emitir_expr(codigo, &pc, dest, op1, op, op2);
        }
    }

    // Corrige offsets dos saltos iflez depois que endereços finais estão calculados
    for (int i = 0; i < num_linhas; i++) {
        if (saltos[i]) {
            char *linha = linhas[i];
            int destino = atoi(strrchr(linha, ' ') + 1); // número da linha de destino do salto
            int delta = enderecos[destino] - (saltos[i] + 4); // cálculo do deslocamento relativo

            // atualiza os 4 bytes do deslocamento no código gerado
            codigo[saltos[i]] = delta & 0xFF;
            codigo[saltos[i] + 1] = (delta >> 8) & 0xFF;
            codigo[saltos[i] + 2] = (delta >> 16) & 0xFF;
            codigo[saltos[i] + 3] = (delta >> 24) & 0xFF;
        }
    }

    return (funcp)codigo;  // retorna ponteiro para função gerada
}
