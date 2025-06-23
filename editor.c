#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <time.h>

#define MAXIMO_LINHAS   100
#define TAMANHO_LINHA   128
#define EDITAR   100
#define NOME_ARQUIVO_TXT "EditorTexto.txt"

typedef struct {
    int  linha_selecionada;
    char text[TAMANHO_LINHA];
} edit_msg_t;

void criar_arquivo(){
  FILE *arquivo;
  int i;

  arquivo = fopen(NOME_ARQUIVO_TXT, "r");

  
  if (arquivo == NULL) {
    arquivo = fopen(NOME_ARQUIVO_TXT, "w");
    if (arquivo == NULL) {
      printf("Erro ao abrir o arquivo!\n");
    }
    for (i = 1; i <= MAXIMO_LINHAS; i++) {
      fprintf(arquivo, "\n");
    }
  }
  fclose(arquivo);
}

void escrever_na_linha( int linha_alvo, const char *nova_linha) {
  FILE *arquivo = fopen(NOME_ARQUIVO_TXT, "r");
  if (!arquivo) {
      perror("Erro ao abrir o arquivo");
      return;
  }

  char linhas[MAXIMO_LINHAS][TAMANHO_LINHA];
  int i = 0;

  // Lê todas as linhas do arquivo
  while (fgets(linhas[i], TAMANHO_LINHA, arquivo) != NULL && i < MAXIMO_LINHAS) {
      i++;
  }
  fclose(arquivo);

  // Modifica a linha desejada (se existir)
  if (linha_alvo >= 0 && linha_alvo < i) {
      snprintf(linhas[linha_alvo], TAMANHO_LINHA, "%s\n", nova_linha);
  } else {
      printf("Linha %d não existe no arquivo.\n", linha_alvo + 1);
      return;
  }

  // Reescreve o arquivo
  arquivo = fopen(NOME_ARQUIVO_TXT, "w");
  if (!arquivo) {
      perror("Erro ao reabrir o arquivo");
      return;
  }
  for (int j = 0; j < i; j++) {
      fputs(linhas[j], arquivo);
  }
  fclose(arquivo);
}

void ler_linha(int numero_linha) {
  FILE *arquivo = fopen(NOME_ARQUIVO_TXT, "r");
  if (arquivo == NULL) {
      perror("Erro ao abrir o arquivo");
      return;
  }

  char linha[TAMANHO_LINHA];
  int contador = 0;

  // Lê linha por linha até chegar na linha desejada
  while (fgets(linha, TAMANHO_LINHA, arquivo) != NULL) {
      if (contador == numero_linha) {
          if(strlen(linha) > 0){
            printf("Conteúdo da Linha %d: %s\n", numero_linha + 1, linha);  // +1 para mostrar ao usuário
          } else {
            printf("Conteúdo da Linha %d: (vazia)\n", numero_linha + 1);  
          }
          fclose(arquivo);
          return;
      }
      contador++;
  }
  fclose(arquivo);
}

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
    char (*doc)[TAMANHO_LINHA];
    int *locks;
    bool *view_only;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc > 1) N = atoi(argv[1]);
    if (N > MAXIMO_LINHAS) N = MAXIMO_LINHAS;

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
        criar_arquivo();
        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            snprintf(doc[i], TAMANHO_LINHA, "Linha %d: (vazia)", i);
        }
        printf("Documento com %d linhas gerado.\n", N);
    }
    
    for (int i = 0; i < N; i++) {
        MPI_Bcast(doc[i], TAMANHO_LINHA, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

    bool exit_all = false;
    while (!exit_all) {
        //aqui processo quando e editado
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, EDITAR, MPI_COMM_WORLD, &flag, &status);
        while (flag) {
            edit_msg_t msg;
            MPI_Recv(&msg, sizeof(msg), MPI_BYTE, status.MPI_SOURCE, EDITAR, MPI_COMM_WORLD, &status);
            // sinal de saída global
            if (msg.linha_selecionada == -1) {
                exit_all = true;
                break;
            }
            strncpy(doc[msg.linha_selecionada], msg.text, TAMANHO_LINHA);
            printf("[Proc %d] Aplicou edição de %d na linha %d: %s\n",
                   rank, status.MPI_SOURCE, msg.linha_selecionada, msg.text);
            MPI_Iprobe(MPI_ANY_SOURCE, EDITAR, MPI_COMM_WORLD, &flag, &status);
        }
        if (exit_all) break;

        if (!view_only[rank]) {
            printf("Processo %d: Digite o número da linha ou -1 para sair:  ", rank);
            fflush(stdout);
            int linha_selecionada;
            if (scanf("%d", &linha_selecionada) != 1 || linha_selecionada < 0) {
                edit_msg_t out = { .linha_selecionada = -1 };
                for (int dst = 0; dst < size; dst++) {
                    MPI_Send(&out, sizeof(out), MPI_BYTE, dst, EDITAR, MPI_COMM_WORLD);
                }
                break;
            }
            if (linha_selecionada >= N) { printf("Linha errada.\n"); continue; }
            if (__sync_lock_test_and_set(&locks[linha_selecionada],1)) {
                printf("Linha %d sendo editada, teste com outra.\n", linha_selecionada);
                continue;
            }

            // printf("Conteudo atual [%d]: %s\n", linha_selecionada, doc[linha_selecionada]);
            ler_linha(linha_selecionada);
            printf(
              "1) Substituir tudo\n"
              "2) Substituir conteudo\n"
              "3) Inserir antes\n" 
              "4) Inserir depois\n"
              );
            int opt;
            scanf("%d", &opt);
            getchar();

            char buffer[TAMANHO_LINHA];
            char novo_texto[TAMANHO_LINHA];

            if(opt == 1){
              printf("Digite a nova linha: \n");
              fgets(novo_texto, TAMANHO_LINHA, stdin);
              escrever_na_linha(linha_selecionada, novo_texto);
            }

            // switch (opt) {
            //     case 1:
            //         printf("Digite a nova linha: ");
            //         fgets(buffer, TAMANHO_LINHA, stdin);
            //         buffer[strcspn(buffer, "\n")] = '\0';
            //         {
            //             int nw = snprintf(novo_texto, TAMANHO_LINHA, "[%d] ", rank);
            //             snprintf(novo_texto + nw, TAMANHO_LINHA - nw, "%s", buffer);
            //         }
            //         break;
            //     case 2: {
            //         printf("Conteudo a substituir: ");
            //         fgets(buffer, TAMANHO_LINHA, stdin);
            //         buffer[strcspn(buffer, "\n")] = '\0';
            //         char find[TAMANHO_LINHA];
            //         printf("trocar por: ");
            //         fgets(find, TAMANHO_LINHA, stdin);
            //         find[strcspn(find, "\n")] = '\0';
            //         substitui(doc[linha_selecionada], buffer, find, novo_texto, TAMANHO_LINHA);
            //         break;
            //     }
            //     case 3:
            //         printf("Texto a inserir antes: ");
            //         fgets(buffer, TAMANHO_LINHA, stdin);
            //         buffer[strcspn(buffer, "\n")] = '\0';
            //         snprintf(novo_texto, TAMANHO_LINHA, "%s%s", buffer, doc[linha_selecionada]);
            //         break;
            //     case 4:
            //         printf("Texto a inserir depois: ");
            //         fgets(buffer, TAMANHO_LINHA, stdin);
            //         buffer[strcspn(buffer, "\n")] = '\0';
            //         snprintf(novo_texto, TAMANHO_LINHA, "%s%s", doc[linha_selecionada], buffer);
            //         break;
            //     default:
            //         printf("Opção inválida.\n");
            //         strncpy(novo_texto, doc[linha_selecionada], TAMANHO_LINHA);
            // }

            //envia edição
            edit_msg_t out = { .linha_selecionada = linha_selecionada };
            strncpy(out.text, novo_texto, TAMANHO_LINHA);
            strncpy(doc[linha_selecionada], novo_texto, TAMANHO_LINHA);
            for (int dst = 0; dst < size; dst++) {
                MPI_Send(&out, sizeof(out), MPI_BYTE, dst, EDITAR, MPI_COMM_WORLD);
            }
            __sync_lock_release(&locks[linha_selecionada]);
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
