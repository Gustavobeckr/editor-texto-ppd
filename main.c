#include <gtk/gtk.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAXIMO_LINHAS 100
#define TAMANHO_LINHA 128
#define NOME_ARQUIVO_TXT "EditorTexto.txt"

void escrever_na_linha(int linha_alvo, const char *nova_linha);
void ler_linha(int numero_linha);

// Estrutura para armazenar os widgets
typedef struct
{
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

static int my_rank, num_procs, linha_selecionada;
static char conteudo_linha_selecionada[TAMANHO_LINHA];

// Array para simular linhas de texto
static char linhas_texto[MAXIMO_LINHAS][TAMANHO_LINHA];

// Função para adicionar log
void adicionar_log(AppWidgets *app, const char *mensagem)
{
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(app->buffer_logs, &iter);

    // Adicionar timestamp
    char log_completo[512];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(log_completo, sizeof(log_completo),
             "[%02d:%02d:%02d] %s\n",
             tm.tm_hour, tm.tm_min, tm.tm_sec, mensagem);

    gtk_text_buffer_insert(app->buffer_logs, &iter, log_completo, -1);

    // Auto-scroll para o final
    GtkTextMark *mark = gtk_text_buffer_get_insert(app->buffer_logs);
    gtk_text_view_scroll_mark_onscreen(app->textarea_logs, mark);
}

// Função para obter o texto selecionado do combo box
gchar *obter_usuario_selecionado(GtkComboBox *combo)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *usuario = NULL;

    if (gtk_combo_box_get_active_iter(combo, &iter))
    {
        model = gtk_combo_box_get_model(combo);
        if (model)
        {
            gtk_tree_model_get(model, &iter, 0, &usuario, -1);
        }
    }

    return usuario;
}

// Callback para o botão "Inserir Texto"
void on_botao_inserir_texto_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets *)user_data;

    const char *conteudo = gtk_entry_get_text(GTK_ENTRY(app->input_conteudo_linha));

    if (strlen(conteudo) > 0)
    {
        if (linha_selecionada >= 1 && linha_selecionada <= MAXIMO_LINHAS) {
          strcpy(linhas_texto[linha_selecionada - 1], conteudo);
        }
        escrever_na_linha(linha_selecionada, conteudo);

        char log_msg[300];
        snprintf(log_msg, sizeof(log_msg),
                 "Texto inserido na linha %d: '%s'", linha_selecionada, conteudo);
        adicionar_log(app, log_msg);

        // Limpar o campo de entrada
        gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha), "");

        // Mostrar mensagem de sucesso
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Texto inserido com sucesso na linha %d!", linha_selecionada);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    else
    {
        adicionar_log(app, "Erro: Conteúdo vazio não pode ser inserido");

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Por favor, insira algum conteúdo!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

// Callback para o botão "Selecionar Linha"
void on_botao_selecionar_linha_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets *)user_data;

    const char *linha_str = gtk_entry_get_text(GTK_ENTRY(app->input_linha_selecionada));

    if (strlen(linha_str) > 0)
    {
        int linha_num = atoi(linha_str);

        if (linha_num > 0 && linha_num <= MAXIMO_LINHAS)
        {
            // Mostrar o conteúdo da linha selecionada
          ler_linha(linha_num);
          gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha), conteudo_linha_selecionada);
          linha_selecionada = linha_num;


            char log_msg[300];
            snprintf(log_msg, sizeof(log_msg),
                     "Linha %d selecionada: '%s'", linha_num, linhas_texto[linha_num - 1]);
            adicionar_log(app, log_msg);

            ler_linha(linha_num);

            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(app->janela_principal),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "Linha %d carregada para edição:\n'%s'",
                linha_num, conteudo_linha_selecionada);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        else
        {
            adicionar_log(app, "Erro: Número de linha inválido");

            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(app->janela_principal),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_OK,
                "Linha inválida! Use um número entre 1 e %d", MAXIMO_LINHAS);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
    }
    else
    {
        adicionar_log(app, "Erro: Número da linha não informado");

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Por favor, informe o número da linha!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

// Callback para o botão "Enviar Chat"
void on_botao_enviar_chat_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets *)user_data;

    const char *mensagem_chat = gtk_entry_get_text(GTK_ENTRY(app->input_conteudo_linha1));

    // Obter usuário selecionado no combo
    gchar *usuario_ativo = obter_usuario_selecionado(GTK_COMBO_BOX(app->combo_usuario_chat));

    if (strlen(mensagem_chat) > 0)
    {
        char log_msg[400];
        if (usuario_ativo && strlen(usuario_ativo) > 0)
        {
            snprintf(log_msg, sizeof(log_msg),
                     "Chat enviado por %s: '%s'", usuario_ativo, mensagem_chat);
        }
        else
        {
            snprintf(log_msg, sizeof(log_msg),
                     "Chat enviado: '%s'", mensagem_chat);
        }
        adicionar_log(app, log_msg);

        // Limpar o campo de mensagem
        gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha1), "");

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Mensagem de chat enviada com sucesso!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    else
    {
        adicionar_log(app, "Erro: Mensagem de chat vazia");

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Por favor, digite uma mensagem!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    if (usuario_ativo)
    {
        g_free(usuario_ativo);
    }
}

// Callback para fechar a aplicação
void on_janela_principal_destroy(GtkWidget *widget, gpointer user_data)
{
    gtk_main_quit();
}

// Função para configurar o combo box de usuários
void configurar_combo_usuarios(AppWidgets *app)
{
    // Criar um ListStore para o combo box
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
    GtkTreeIter iter;

    // Adicionar usuários ao store
    const char *usuarios[] = {"Usuario1", "Usuario2", "Usuario3", "Admin", "Convidado"};
    int num_usuarios = sizeof(usuarios) / sizeof(usuarios[0]);

    for (int i = 0; i < num_usuarios; i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, usuarios[i], -1);
    }

    // Configurar o modelo do combo box
    gtk_combo_box_set_model(GTK_COMBO_BOX(app->combo_usuario_chat), GTK_TREE_MODEL(store));

    // Criar um cell renderer para exibir o texto
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(app->combo_usuario_chat), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(app->combo_usuario_chat), renderer, "text", 0, NULL);

    // Selecionar o primeiro item por padrão
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_usuario_chat), 0);

    // Liberar referência do store
    g_object_unref(store);
}
void criar_arquivo()
{
    FILE *arquivo;
    int i;

    arquivo = fopen(NOME_ARQUIVO_TXT, "r");

    if (arquivo == NULL)
    {
        arquivo = fopen(NOME_ARQUIVO_TXT, "w");
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

void escrever_na_linha(int linha_alvo, const char *nova_linha)
{
    FILE *arquivo = fopen(NOME_ARQUIVO_TXT, "r");
    if (!arquivo)
    {
        perror("Erro ao abrir o arquivo");
        return;
    }

    char linhas[MAXIMO_LINHAS][TAMANHO_LINHA];
    int i = 0;

    // Lê todas as linhas do arquivo
    while (fgets(linhas[i], TAMANHO_LINHA, arquivo) != NULL && i < MAXIMO_LINHAS)
    {
        i++;
    }
    fclose(arquivo);

    // Modifica a linha desejada (se existir)
    if (linha_alvo >= 0 && linha_alvo < i)
    {
        snprintf(linhas[linha_alvo - 1], TAMANHO_LINHA, "%s\n", nova_linha);
    }
    else
    {
        printf("Linha %d não existe no arquivo.\n", linha_alvo + 1);
        return;
    }

    // Reescreve o arquivo
    arquivo = fopen(NOME_ARQUIVO_TXT, "w");
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

void ler_linha(int numero_linha)
{
    FILE *arquivo = fopen(NOME_ARQUIVO_TXT, "r");
    if (arquivo == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return;
    }

    char linha[TAMANHO_LINHA];
    int contador = 1;

    // Lê linha por linha até chegar na linha desejada
    while (fgets(linha, TAMANHO_LINHA, arquivo) != NULL)
    {
        if (contador == numero_linha)
        {
            if (strlen(linha) > 0)
            {
                strcpy(conteudo_linha_selecionada, linha);
                conteudo_linha_selecionada[strcspn(conteudo_linha_selecionada, "\n")] = '\0';  // remove o \n

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
    // Inicializa MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    char titulo_janela_principal[100];
    snprintf(titulo_janela_principal, sizeof(titulo_janela_principal), "Editor de Texto PPD - Processo %d", my_rank);

    GtkBuilder *builder;
    AppWidgets *app;
    GError *error = NULL;

    if (my_rank == 0)
    {
        criar_arquivo();
    }

    // Inicializar GTK
    gtk_init(&argc, &argv);

    // Alocar memória para a estrutura de widgets
    app = g_malloc(sizeof(AppWidgets));

    // Criar builder e carregar o arquivo .glade
    builder = gtk_builder_new();
    if (!gtk_builder_add_from_file(builder, "glade_layout.glade", &error))
    {
        g_printerr("Erro ao carregar arquivo .glade: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    // Obter widgets do builder
    app->janela_principal = GTK_WIDGET(gtk_builder_get_object(builder, "janela_principal"));
    app->botao_inserir_texto = GTK_WIDGET(gtk_builder_get_object(builder, "botao_inserir_texto"));
    app->botao_selecionar_linha = GTK_WIDGET(gtk_builder_get_object(builder, "botao_selecionar_linha"));
    app->input_linha_selecionada = GTK_WIDGET(gtk_builder_get_object(builder, "input_linha_selecionada"));
    app->input_conteudo_linha = GTK_WIDGET(gtk_builder_get_object(builder, "input_conteudo_linha"));
    app->botao_enviar_chat = GTK_WIDGET(gtk_builder_get_object(builder, "botao_enviar_chat"));
    app->combo_usuario_chat = GTK_WIDGET(gtk_builder_get_object(builder, "combo_usuario_chat"));
    app->input_conteudo_linha1 = GTK_WIDGET(gtk_builder_get_object(builder, "input_conteudo_linha1"));
    app->textarea_logs = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "textarea_logs"));

    gtk_window_set_title(GTK_WINDOW(app->janela_principal), titulo_janela_principal);

    // Configurar o buffer de texto para logs
    app->buffer_logs = gtk_text_view_get_buffer(app->textarea_logs);

    // Conectar sinais aos callbacks
    g_signal_connect(app->janela_principal, "destroy",
                     G_CALLBACK(on_janela_principal_destroy), NULL);
    g_signal_connect(app->botao_inserir_texto, "clicked",
                     G_CALLBACK(on_botao_inserir_texto_clicked), app);
    g_signal_connect(app->botao_selecionar_linha, "clicked",
                     G_CALLBACK(on_botao_selecionar_linha_clicked), app);
    g_signal_connect(app->botao_enviar_chat, "clicked",
                     G_CALLBACK(on_botao_enviar_chat_clicked), app);

    // Configurar combo box de usuários
    configurar_combo_usuarios(app);

    // Adicionar log inicial
    adicionar_log(app, "Editor inicializado com sucesso");

    // Liberar o builder
    g_object_unref(builder);

    // Mostrar a janela principal
    gtk_widget_show_all(app->janela_principal);

    // Iniciar o loop principal do GTK
    gtk_main();

    // Liberar memória
    g_free(app);

    // Finaliza MPI
    MPI_Finalize();

    return 0;
}
