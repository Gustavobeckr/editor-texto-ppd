#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <time.h>

#define linha_max 1000  
#define tam   128
#define editar   100

typedef struct {
    int  line;
    char text[tam];
} edit_msg_t;

static void substitui(const char *orig, const char *find, const char *rep, char *out, size_t out_sz) {
    char *pos = strstr(orig, find);
    if (!pos) {
        strncpy(out, orig, out_sz);
        return;
    }
    size_t prefix = pos - orig;
    size_t find_len = strlen(find);
    snprintf(out, out_sz, "%.*s%s%s",
             (int)prefix, orig,
             rep,
             pos + find_len);
}

int main(int argc, char *argv[]) {
    int rank, size, N = 1000;
    char (*doc)[tam];
    int *locks;
    bool *view_only;
    MPI_Status status;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc > 1) N = atoi(argv[1]);
    if (N > linha_max) N = linha_max;

    doc   = malloc(sizeof(*doc) * N);
    locks = calloc(N, sizeof(int));
    view_only = calloc(size, sizeof(bool));

    //com x views
    if (argc > 2 && strncmp(argv[2], "view=", 5) == 0) {
        char *p = argv[2] + 5;
        while (*p) {
            int r = strtol(p, &p, 10);
            if (r >= 0 && r < size) view_only[r] = true;
            if (*p == ',') p++;
        }
    }
    if (rank == 0) {
        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            snprintf(doc[i], tam, "Linha %d: (vazia)", i);
        }
        printf("Documento com %d linhas gerado.\n", N);
    }
    
    for (int i = 0; i < N; i++) {
        MPI_Bcast(doc[i], tam, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

    bool exit_all = false;
    while (!exit_all) {
        //aqui processo quando e editado
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, editar, MPI_COMM_WORLD, &flag, &status);
        while (flag) {
            edit_msg_t msg;
            MPI_Recv(&msg, sizeof(msg), MPI_BYTE, status.MPI_SOURCE, editar, MPI_COMM_WORLD, &status);
            // sinal de saída global
            if (msg.line == -1) {
                exit_all = true;
                break;
            }
            strncpy(doc[msg.line], msg.text, tam);
            printf("[Proc %d] Aplicou edição de %d na linha %d: %s\n",
                   rank, status.MPI_SOURCE, msg.line, msg.text);
            MPI_Iprobe(MPI_ANY_SOURCE, editar, MPI_COMM_WORLD, &flag, &status);
        }
        if (exit_all) break;

        if (!view_only[rank]) {
            printf("\nProcesso %d num da linha ou -1 para sair: ", rank, N-1);
            fflush(stdout);
            int line;
            if (scanf("%d", &line) != 1 || line < 0) {
                edit_msg_t out = { .line = -1 };
                for (int dst = 0; dst < size; dst++) {
                    MPI_Send(&out, sizeof(out), MPI_BYTE, dst, editar, MPI_COMM_WORLD);
                }
                break;
            }
            if (line >= N) { printf("Linha errada.\n"); continue; }
            if (__sync_lock_test_and_set(&locks[line],1)) {
                printf("Linha %d sendo editada, teste com outra.\n", line);
                continue;
            }

            printf("Conteudo atual [%d]: %s\n", line, doc[line]);
            printf(" 1)Substituir tudo\n 2) Substituir conteudo\n 3) Inserir antes\n 4) Inserir depois\nEscolha (1-4): ");
            int opt;
            scanf("%d", &opt);
            getchar();

            char buffer[tam];
            char newtext[tam];

            switch (opt) {
                case 1:
                    printf("Digite a nova linha: ");
                    fgets(buffer, tam, stdin);
                    buffer[strcspn(buffer, "\n")] = '\0';
                    {
                        int nw = snprintf(newtext, tam, "[%d] ", rank);
                        snprintf(newtext + nw, tam - nw, "%s", buffer);
                    }
                    break;
                case 2: {
                    printf("Conteudo a substituir: ");
                    fgets(buffer, tam, stdin);
                    buffer[strcspn(buffer, "\n")] = '\0';
                    char find[tam];
                    printf("trocar por: ");
                    fgets(find, tam, stdin);
                    find[strcspn(find, "\n")] = '\0';
                    substitui(doc[line], buffer, find, newtext, tam);
                    break;
                }
                case 3:
                    printf("Texto a inserir antes: ");
                    fgets(buffer, tam, stdin);
                    buffer[strcspn(buffer, "\n")] = '\0';
                    snprintf(newtext, tam, "%s%s", buffer, doc[line]);
                    break;
                case 4:
                    printf("Texto a inserir depois: ");
                    fgets(buffer, tam, stdin);
                    buffer[strcspn(buffer, "\n")] = '\0';
                    snprintf(newtext, tam, "%s%s", doc[line], buffer);
                    break;
                default:
                    printf("Opção inválida.\n");
                    strncpy(newtext, doc[line], tam);
            }

            //envia edição
            edit_msg_t out = { .line = line };
            strncpy(out.text, newtext, tam);
            strncpy(doc[line], newtext, tam);
            for (int dst = 0; dst < size; dst++) {
                MPI_Send(&out, sizeof(out), MPI_BYTE, dst, editar, MPI_COMM_WORLD);
            }
            __sync_lock_release(&locks[line]);
        } else {
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
            nanosleep(&ts, NULL);
        }
    }

    free(doc);
    free(locks);
    free(view_only);
    MPI_Finalize();
    return 0;
}
