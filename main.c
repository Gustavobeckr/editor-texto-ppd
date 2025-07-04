#include <omp.h>
#include <gtk/gtk.h>
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAXIMO_LINHAS 100
#define TAMANHO_LINHA 128

void escrever_na_linha(int linha_alvo, const char *nova_linha);
void ler_linha(int numero_linha);
void escutar_mudancas_de_dono(void);


typedef struct {
    GtkWidget *janela_principal;
    GtkWidget *botao_inserir_texto;
    GtkWidget *botao_selecionar_linha;
    GtkWidget *input_linha_selecionada;
    GtkWidget *input_conteudo_linha;
    GtkWidget *botao_enviar_chat;
    GtkWidget *combo_usuario_chat;
    GtkWidget *input_conteudo_linha1;
    GtkTextView *textarea_logs;
    GtkTextBuffer *buffer_logs;
} AppWidgets;

static const char *arquivo_uso = NULL;
static int my_rank, num_procs, linha_selecionada = 0;
static int dono_linha[MAXIMO_LINHAS] = { -1 };
static char conteudo_linha_selecionada[TAMANHO_LINHA];
static char linhas_texto[MAXIMO_LINHAS][TAMANHO_LINHA];
static int rank_alvo_comunicacao = 0; 

void* escutar_chat_em_background(void* arg) {
    AppWidgets *app = (AppWidgets*) arg;
    char mensagem_recebida[128];
    MPI_Status status;

    while (1) {    
        MPI_Recv(&mensagem_recebida, 128, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        GtkTextIter start, end;
        gtk_text_buffer_get_start_iter(app->buffer_logs, &start);
        gtk_text_buffer_get_end_iter(app->buffer_logs, &end);
        gchar *texto_antigo = gtk_text_buffer_get_text(app->buffer_logs, &start, &end, FALSE);

        gchar *novo_texto = g_strdup_printf("%s\n[Recebido] %s", texto_antigo, mensagem_recebida);
        gtk_text_buffer_set_text(app->buffer_logs, novo_texto, -1);
        g_free(texto_antigo);
        g_free(novo_texto);   
    }

    return NULL;
}

//adicao dos logs na tela
void adicionar_log(AppWidgets *app, const char *mensagem) {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(app->buffer_logs, &iter);
    char log_completo[512];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(log_completo, sizeof(log_completo), "[%02d:%02d:%02d] %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, mensagem);
    gtk_text_buffer_insert(app->buffer_logs, &iter, log_completo, -1);
    GtkTextMark *mark = gtk_text_buffer_get_insert(app->buffer_logs);
    gtk_text_view_scroll_mark_onscreen(app->textarea_logs, mark);
}

gchar *obter_usuario_selecionado(GtkComboBox *combo) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *usuario = NULL;

    rank_alvo_comunicacao = gtk_combo_box_get_active(combo);

    if (gtk_combo_box_get_active_iter(combo, &iter)) {
        model = gtk_combo_box_get_model(combo);
        gtk_tree_model_get(model, &iter, 0, &usuario, -1);
    }
    return usuario;
}

//insere o texto
void on_botao_inserir_texto_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *app = (AppWidgets *)user_data;
    escutar_mudancas_de_dono();
    const char *conteudo = gtk_entry_get_text(GTK_ENTRY(app->input_conteudo_linha));
    if (linha_selecionada < 1 || linha_selecionada > MAXIMO_LINHAS || strlen(conteudo) == 0) return;
    int idx = linha_selecionada - 1;
    if (dono_linha[idx] != my_rank && dono_linha[idx] != -1) {
        char aviso[300];
        snprintf(aviso, sizeof(aviso), "Linha %d em uso por processo %d.", linha_selecionada, dono_linha[idx]);
        adicionar_log(app, aviso);
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->janela_principal), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", aviso);
        gtk_dialog_run(GTK_DIALOG(dialog)); gtk_widget_destroy(dialog);
        return;
    }
    //escreve no txt e avisa
    strcpy(linhas_texto[idx], conteudo);
    escrever_na_linha(linha_selecionada, conteudo);
    int dados[2] = { idx, my_rank };
    for (int i = 0; i < num_procs; i++) if (i != my_rank) MPI_Send(dados, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
    char log_msg[300];
    snprintf(log_msg, sizeof(log_msg), "Texto salvo na linha %d: '%s'", linha_selecionada, conteudo);
    adicionar_log(app, log_msg);
    gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha), "");
    //depois de salvar, libera a linha para editar novamente
    dono_linha[idx] = -1;
    int dados_release[2] = { idx, -1 };
    for (int i = 0; i < num_procs; i++) if (i != my_rank) MPI_Send(dados_release, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
}

//evento do click na linha
void on_botao_selecionar_linha_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *app = (AppWidgets *)user_data;
    escutar_mudancas_de_dono();
    const char *linha_str = gtk_entry_get_text(GTK_ENTRY(app->input_linha_selecionada));
    if (!linha_str || strlen(linha_str) == 0) return;
    int nova_linha = atoi(linha_str);
    if (nova_linha < 1 || nova_linha > MAXIMO_LINHAS) return;
    int idx = nova_linha - 1;
    if (dono_linha[idx] != -1 && dono_linha[idx] != my_rank) {
        char aviso[256];
        snprintf(aviso, sizeof(aviso), "Linha %d em uso por processo %d.", nova_linha, dono_linha[idx]);
        adicionar_log(app, aviso);
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->janela_principal), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", aviso);
        gtk_dialog_run(GTK_DIALOG(dialog)); gtk_widget_destroy(dialog);
        return;
    }
    //se nao ta editada, libera a linha
    if (linha_selecionada > 0 && linha_selecionada != nova_linha) {
        int old_idx = linha_selecionada - 1;
        if (dono_linha[old_idx] == my_rank && strlen(linhas_texto[old_idx]) == 0) {
            dono_linha[old_idx] = -1;
            int dados_release[2] = { old_idx, -1 };
            for (int i = 0; i < num_procs; i++) if (i != my_rank) MPI_Send(dados_release, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }
    linha_selecionada = nova_linha;
    dono_linha[idx] = my_rank;
    int dados_lock[2] = { idx, my_rank };
    for (int i = 0; i < num_procs; i++) if (i != my_rank) MPI_Send(dados_lock, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
    ler_linha(nova_linha);
    gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha), conteudo_linha_selecionada);
    gtk_editable_set_editable(GTK_EDITABLE(app->input_conteudo_linha), TRUE);
    char msg[256]; snprintf(msg, sizeof(msg), "Linha %d pronta para edição", nova_linha);
    adicionar_log(app, msg);
}

//escuta as mensagens do chat
void escutar_mudancas_de_dono() {
    MPI_Status status; int dados[2], flag;
    do {
        MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            MPI_Recv(dados, 2, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int linha = dados[0], novo_dono = dados[1];
            dono_linha[linha] = novo_dono;
        }
    } while (flag);
}

//evento do click para enviar mensagem no chat
void on_botao_enviar_chat_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets *) user_data;
    const char *texto = gtk_entry_get_text(GTK_ENTRY(app->input_conteudo_linha1));

    if (strlen(texto) == 0) {
        adicionar_log(app, "Erro: Mensagem de chat vazia");
        GtkWidget *dlg = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Por favor, digite uma mensagem!");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return;
    }

//monta o texto do chat
    char mensagem_chat[128];
    gchar *usuario = obter_usuario_selecionado(GTK_COMBO_BOX(app->combo_usuario_chat));
    if (usuario && strlen(usuario) > 0) {
        snprintf(mensagem_chat, sizeof(mensagem_chat), "[%s] %s", usuario, texto);
        g_free(usuario);
    } else {
        snprintf(mensagem_chat, sizeof(mensagem_chat), "%s", texto);
    }

    adicionar_log(app, mensagem_chat);
    gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha1), "");
        MPI_Send(
            mensagem_chat,
            (int)strlen(mensagem_chat) + 1,
            MPI_CHAR,
            rank_alvo_comunicacao,
            99,
            MPI_COMM_WORLD
        );
    
    GtkWidget *dlg = gtk_message_dialog_new(
        GTK_WINDOW(app->janela_principal),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Mensagem de chat enviada com sucesso!");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

void on_janela_principal_destroy(GtkWidget *widget, gpointer user_data)
{
    gtk_main_quit();
}
//configs dos users
void configurar_combo_usuarios(AppWidgets *app)
{
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
    GtkTreeIter iter;
    char nomeCombo[150];
    char strUsuario[100] = "Processo ";

    for (int i = 0; i < num_procs ; i++)
    {
        sprintf(nomeCombo, "%s%d", strUsuario, i);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, nomeCombo, -1);


    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(app->combo_usuario_chat), GTK_TREE_MODEL(store));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(app->combo_usuario_chat), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(app->combo_usuario_chat), renderer, "text", 0, NULL);

    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_usuario_chat), 0);

    g_object_unref(store);
}

//funcao que cria o arquivo
void criar_arquivo()
{
    FILE *arquivo;
    int i;

    arquivo = fopen(arquivo_uso, "r");

    if (arquivo == NULL)
    {
        arquivo = fopen(arquivo_uso, "w");
        if (arquivo == NULL)
        {
            printf("Erro ao abrir o arquivo!\n");
        }
        for (i = 1; i <= MAXIMO_LINHAS; i++)
        {
            fprintf(arquivo, "\n");
        }
    }
    fclose(arquivo);
}

//funcao que escreve na linha selecionada
void escrever_na_linha(int linha_alvo, const char *nova_linha)
{
    FILE *arquivo = fopen(arquivo_uso, "r");
    if (!arquivo)
    {
        perror("Erro ao abrir o arquivo");
        return;
    }

    char linhas[MAXIMO_LINHAS][TAMANHO_LINHA];
    int i = 0;

    while (fgets(linhas[i], TAMANHO_LINHA, arquivo) != NULL && i < MAXIMO_LINHAS)
    {
        i++;
    }
    fclose(arquivo);

    if (linha_alvo >= 0 && linha_alvo < i)
    {
        snprintf(linhas[linha_alvo - 1], TAMANHO_LINHA, "%s\n", nova_linha);
    }
    else
    {
        printf("Linha %d não existe no arquivo.\n", linha_alvo + 1);
        return;
    }

    arquivo = fopen(arquivo_uso, "w");
    if (!arquivo)
    {
        perror("Erro ao reabrir o arquivo");
        return;
    }
    for (int j = 0; j < i; j++)
    {
        fputs(linhas[j], arquivo);
    }
    fclose(arquivo);
}

//gerar para teste
void gerar_dados_para_teste(const char *nome_arquivo, int num_linhas) {
    const char *temp = "EditorTeste.txt";
    FILE *f = fopen(temp, "w");
    if (!f) {
        perror("Erro ao criar arquivo de teste");
        return;
    }

    double t0 = omp_get_wtime();

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < num_linhas; i++) {
        char buf[TAMANHO_LINHA];
        snprintf(buf, sizeof(buf),
                 "Linha %d gerada pela thread %d\n",
                 i, omp_get_thread_num());
        #pragma omp critical
        {
            fputs(buf, f);
        }
    }

    double t1 = omp_get_wtime();
    printf("Geradas %d linhas em %.3f s com %d threads\n",
           num_linhas, t1 - t0, omp_get_max_threads());

    fclose(f);
}

//funcao que le o que tem na linha selecionda
void ler_linha(int numero_linha)
{
    FILE *arquivo = fopen(arquivo_uso, "r");
    if (arquivo == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return;
    }

    char linha[TAMANHO_LINHA];
    int contador = 1;

    while (fgets(linha, TAMANHO_LINHA, arquivo) != NULL)
    {
        if (contador == numero_linha)
        {
            if (strlen(linha) > 0)
            {
                strcpy(conteudo_linha_selecionada, linha);
                conteudo_linha_selecionada[strcspn(conteudo_linha_selecionada, "\n")] = '\0'; 
                strcpy(linhas_texto[numero_linha-1], conteudo_linha_selecionada);
            }
            else
            {
                printf("NADA");
                return;
            }
            fclose(arquivo);

            return;
        }
        contador++;
    }
    fclose(arquivo);
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    int num_test = 0;
    if (argc >= 2) num_test = atoi(argv[1]);

    if (num_test > 0) {
        arquivo_uso = "EditorTeste.txt";
        if (my_rank == 0) {
            gerar_dados_para_teste(arquivo_uso, num_test);
        }
    } else {
        arquivo_uso = "EditorTexto.txt";
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (num_test == 0 && my_rank == 0) {
        criar_arquivo(); 
        for (int i = 0; i < MAXIMO_LINHAS; i++)
            dono_linha[i] = -1;
    }
    MPI_Bcast(dono_linha, MAXIMO_LINHAS, MPI_INT, 0, MPI_COMM_WORLD);
        char titulo_janela_principal[100];
        snprintf(titulo_janela_principal, sizeof(titulo_janela_principal),
                "Editor de Texto PPD - Processo %d", my_rank);

    gtk_init(&argc, &argv);
    AppWidgets *app = g_malloc(sizeof(AppWidgets));

    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;
    if (!gtk_builder_add_from_file(builder, "glade_layout.glade", &error)) {
        g_printerr("Erro ao carregar .glade: %s\n", error->message);
        g_error_free(error);
        return 1;
    }
    app->janela_principal      = GTK_WIDGET(gtk_builder_get_object(builder, "janela_principal"));
    app->botao_inserir_texto   = GTK_WIDGET(gtk_builder_get_object(builder, "botao_inserir_texto"));
    app->botao_selecionar_linha= GTK_WIDGET(gtk_builder_get_object(builder, "botao_selecionar_linha"));
    app->input_linha_selecionada= GTK_WIDGET(gtk_builder_get_object(builder, "input_linha_selecionada"));
    app->input_conteudo_linha  = GTK_WIDGET(gtk_builder_get_object(builder, "input_conteudo_linha"));
    app->botao_enviar_chat     = GTK_WIDGET(gtk_builder_get_object(builder, "botao_enviar_chat"));
    app->combo_usuario_chat    = GTK_WIDGET(gtk_builder_get_object(builder, "combo_usuario_chat"));
    app->input_conteudo_linha1 = GTK_WIDGET(gtk_builder_get_object(builder, "input_conteudo_linha1"));
    app->textarea_logs         = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "textarea_logs"));

    gtk_window_set_title(GTK_WINDOW(app->janela_principal),
                         titulo_janela_principal);

    app->buffer_logs = gtk_text_view_get_buffer(app->textarea_logs);

    pthread_t thread_chat;
    pthread_create(&thread_chat,
                   NULL,
                   escutar_chat_em_background,
                   app);

    g_signal_connect(app->janela_principal, "destroy",
                     G_CALLBACK(on_janela_principal_destroy), NULL);
    g_signal_connect(app->botao_inserir_texto, "clicked",
                     G_CALLBACK(on_botao_inserir_texto_clicked), app);
    g_signal_connect(app->botao_selecionar_linha, "clicked",
                     G_CALLBACK(on_botao_selecionar_linha_clicked), app);
    g_signal_connect(app->botao_enviar_chat, "clicked",
                     G_CALLBACK(on_botao_enviar_chat_clicked), app);

    configurar_combo_usuarios(app);
    adicionar_log(app, "Editor inicializado com sucesso");

    g_object_unref(builder);
    gtk_widget_show_all(app->janela_principal);
    gtk_main();

//limpa e finaliza
    g_free(app);
    MPI_Finalize();
    return 0;
}
