/* Luan Carlos Almada Braga 2411776 Turma 3WA */
/* Bruno Tardin Fernandes 2411072 Turma 3WA */

// testapeqcomp.c
// Executa 10 testes SBas (t1.sbas–t10.sbas)

#include <stdio.h>
#include <stdlib.h>
#include "peqcomp.h"

int main() {
    const char *arquivos[10] = {
        "t1.sbas", "t2.sbas", "t3.sbas", "t4.sbas", "t5.sbas",
        "t6.sbas", "t7.sbas", "t8.sbas", "t9.sbas", "t10.sbas"
    };

    // Argumentos para p1 e p2 (se necessário)
    int p1[10] = { 0, 5, 3, 10, 2, -5, 5, 7, -3, 3 };
    int p2[10] = { 0, 7, 4, 3, 10, 0, 0, 0, 0, 0 };

    // Quantos argumentos cada função espera
    int nargs[10] = { 0, 2, 2, 2, 2, 1, 1, 1, 1, 1 };

    unsigned char codigo[4096];

    for (int i = 0; i < 10; i++) {
        FILE *f = fopen(arquivos[i], "r");
        if (!f) {
            perror(arquivos[i]);
            continue;
        }

        funcp funcao = peqcomp(f, codigo);
        fclose(f);

        int resultado = 0;

        if (nargs[i] == 0) {
            resultado = funcao();
        } else if (nargs[i] == 1) {
            resultado = funcao(p1[i]);
        } else if (nargs[i] == 2) {
            resultado = funcao(p1[i], p2[i]);
        }

        printf("Teste %d (%s): resultado = %d\n", i + 1, arquivos[i], resultado);
    }

    return 0;
}
