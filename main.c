#include <gtk/gtk.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>

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

static int my_rank, num_procs;

// Array para simular linhas de texto
static char linhas_texto[100][256];
static int total_linhas = 0;

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

// Callback para o botão "Inserir Texto"
void on_botao_inserir_texto_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *app = (AppWidgets *)user_data;

    const char *conteudo = gtk_entry_get_text(GTK_ENTRY(app->input_conteudo_linha));

    if (strlen(conteudo) > 0)
    {
        strcpy(linhas_texto[total_linhas], conteudo);
        total_linhas++;

        char log_msg[300];
        snprintf(log_msg, sizeof(log_msg),
                 "Texto inserido na linha %d: '%s'", total_linhas, conteudo);
        adicionar_log(app, log_msg);

        // Limpar o campo de entrada
        gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha), "");

        // Mostrar mensagem de sucesso
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->janela_principal),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Texto inserido com sucesso na linha %d!", total_linhas);
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

        if (linha_num > 0 && linha_num <= total_linhas)
        {
            // Mostrar o conteúdo da linha selecionada
            gtk_entry_set_text(GTK_ENTRY(app->input_conteudo_linha),
                               linhas_texto[linha_num - 1]);

            char log_msg[300];
            snprintf(log_msg, sizeof(log_msg),
                     "Linha %d selecionada: '%s'", linha_num, linhas_texto[linha_num - 1]);
            adicionar_log(app, log_msg);

            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(app->janela_principal),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "Linha %d carregada para edição:\n'%s'",
                linha_num, linhas_texto[linha_num - 1]);
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
                "Linha inválida! Use um número entre 1 e %d", total_linhas);
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
    gchar *usuario_ativo = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(app->combo_usuario_chat));

    if (strlen(mensagem_chat) > 0)
    {
        char log_msg[400];
        if (usuario_ativo)
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
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Usuario1", -1);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Usuario2", -1);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Usuario3", -1);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Admin", -1);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Convidado", -1);

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