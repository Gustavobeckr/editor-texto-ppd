#include <gtk/gtk.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>

// Widgets globais
static GtkWidget *main_window;
static GtkWidget *text_view;
static GtkWidget *log_text_view;
static GtkTextBuffer *text_buffer;
static GtkTextBuffer *log_buffer;
static GtkWidget *chat_window = NULL;
static int my_rank, num_procs;

// Função para adicionar log
static void add_log(const char *message) {
    GtkTextIter end_iter;
    char timestamp_msg[512];
    
    // Adiciona timestamp simples
    snprintf(timestamp_msg, sizeof(timestamp_msg), "[Processo %d] %s\n", my_rank, message);
    
    gtk_text_buffer_get_end_iter(log_buffer, &end_iter);
    gtk_text_buffer_insert(log_buffer, &end_iter, timestamp_msg, -1);
    
    // Auto-scroll para o final
    GtkTextMark *mark = gtk_text_buffer_get_insert(log_buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(log_text_view), mark);
}

// Função chamada quando a janela principal é fechada
static void on_main_window_destroy() {
    if (chat_window) {
        gtk_widget_destroy(chat_window);
    }
    gtk_main_quit();
}

// Função para o botão "Alterar Texto"
static void on_alter_text_clicked(GtkWidget *widget, gpointer data) {
    add_log("Botão 'Alterar Texto' clicado");
    
    // Obtém o texto atual
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(text_buffer, &start, &end);
    char *current_text = gtk_text_buffer_get_text(text_buffer, &start, &end, FALSE);
    
    // Adiciona texto de exemplo ou modifica o existente
    char new_text[1024];
    if (strlen(current_text) == 0) {
        snprintf(new_text, sizeof(new_text), "Texto alterado pelo processo %d\nHorário: %ld", my_rank, time(NULL));
    } else {
        snprintf(new_text, sizeof(new_text), "%s\n[Modificado pelo processo %d]", current_text, my_rank);
    }
    
    gtk_text_buffer_set_text(text_buffer, new_text, -1);
    g_free(current_text);
    
    add_log("Texto alterado com sucesso");
}

// Função chamada quando a janela de chat é fechada
static void on_chat_window_destroy() {
    chat_window = NULL;
    add_log("Janela de chat fechada");
}

// Função para enviar mensagem no chat
static void on_send_message_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget **chat_widgets = (GtkWidget **)user_data;
    GtkWidget *user_entry = chat_widgets[0];
    GtkWidget *message_entry = chat_widgets[1];
    GtkWidget *chat_display = chat_widgets[2];
    
    const char *user_text = gtk_entry_get_text(GTK_ENTRY(user_entry));
    const char *message_text = gtk_entry_get_text(GTK_ENTRY(message_entry));
    
    if (strlen(user_text) == 0 || strlen(message_text) == 0) {
        add_log("Erro: Usuário e mensagem devem ser preenchidos");
        return;
    }
    
    // Adiciona mensagem ao display do chat
    GtkTextBuffer *chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_display));
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(chat_buffer, &end_iter);
    
    char chat_message[512];
    snprintf(chat_message, sizeof(chat_message), "Para %s: %s\n", user_text, message_text);
    gtk_text_buffer_insert(chat_buffer, &end_iter, chat_message, -1);
    
    // Limpa os campos de entrada
    gtk_entry_set_text(GTK_ENTRY(message_entry), "");
    
    // Log da ação
    char log_message[512];
    snprintf(log_message, sizeof(log_message), "Mensagem enviada para %s: %s", user_text, message_text);
    add_log(log_message);
    
    // Aqui você pode adicionar lógica MPI para enviar a mensagem real
    // Exemplo de comunicação MPI (comentado para não interferir):
    /*
    int target_rank = atoi(user_text);
    if (target_rank >= 0 && target_rank < num_procs && target_rank != my_rank) {
        MPI_Send((void*)message_text, strlen(message_text)+1, MPI_CHAR, target_rank, 0, MPI_COMM_WORLD);
    }
    */
}

// Função para o botão "Chat"
static void on_chat_clicked(GtkWidget *widget, gpointer data) {
    add_log("Abrindo janela de chat");
    
    // Se a janela já existe, apenas a traz para frente
    if (chat_window) {
        gtk_window_present(GTK_WINDOW(chat_window));
        return;
    }
    
    // Cria nova janela de chat
    chat_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(chat_window), "Chat - Interface de Mensagens");
    gtk_window_set_default_size(GTK_WINDOW(chat_window), 500, 400);
    gtk_window_set_transient_for(GTK_WINDOW(chat_window), GTK_WINDOW(main_window));
    
    // Container principal do chat
    GtkWidget *chat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(chat_vbox), 10);
    gtk_container_add(GTK_CONTAINER(chat_window), chat_vbox);
    
    // Área de exibição do chat
    GtkWidget *chat_display = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_display), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_display), GTK_WRAP_WORD);
    
    GtkWidget *chat_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chat_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(chat_scroll), chat_display);
    gtk_widget_set_size_request(chat_scroll, -1, 200);
    
    // Campos de entrada
    GtkWidget *input_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(input_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(input_grid), 5);
    
    // Campo para usuário destino
    GtkWidget *user_label = gtk_label_new("Usuário (Rank):");
    gtk_widget_set_halign(user_label, GTK_ALIGN_START);
    GtkWidget *user_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(user_entry), "Digite o rank do usuário (0, 1, 2...)");
    
    // Campo para mensagem
    GtkWidget *message_label = gtk_label_new("Mensagem:");
    gtk_widget_set_halign(message_label, GTK_ALIGN_START);
    GtkWidget *message_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(message_entry), "Digite sua mensagem aqui...");
    
    // Botão enviar
    GtkWidget *send_button = gtk_button_new_with_label("Enviar Mensagem");
    
    // Array para passar múltiplos widgets para o callback
    static GtkWidget *chat_widgets[3];
    chat_widgets[0] = user_entry;
    chat_widgets[1] = message_entry;
    chat_widgets[2] = chat_display;
    
    // Organiza o grid
    gtk_grid_attach(GTK_GRID(input_grid), user_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), user_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), message_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), message_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), send_button, 0, 2, 2, 1);
    
    // Adiciona widgets ao container principal
    gtk_box_pack_start(GTK_BOX(chat_vbox), gtk_label_new("Histórico do Chat:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(chat_vbox), chat_scroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(chat_vbox), input_grid, FALSE, FALSE, 0);
    
    // Conecta sinais
    g_signal_connect(chat_window, "destroy", G_CALLBACK(on_chat_window_destroy), NULL);
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_message_clicked), chat_widgets);
    
    // Permite enviar com Enter no campo de mensagem
    g_signal_connect(message_entry, "activate", G_CALLBACK(on_send_message_clicked), chat_widgets);
    
    // Mostra a janela
    gtk_widget_show_all(chat_window);
}

int main(int argc, char *argv[]) {
    // Inicializa MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    
    // Inicializa o GTK
    gtk_init(&argc, &argv);
    
    // Cria janela principal
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    char title[100];
    snprintf(title, sizeof(title), "Interface MPI+GTK - Processo %d", my_rank);
    gtk_window_set_title(GTK_WINDOW(main_window), title);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 600, 500);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    
    // Container principal
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(main_window), main_vbox);
    
    // === BARRA SUPERIOR COM BOTÕES ===
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(button_box), 10);
    
    GtkWidget *alter_text_button = gtk_button_new_with_label("Alterar Texto");
    GtkWidget *chat_button = gtk_button_new_with_label("Chat");
    
    gtk_box_pack_start(GTK_BOX(button_box), alter_text_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), chat_button, FALSE, FALSE, 0);
    
    // === ÁREA DE TEXTO PRINCIPAL (BODY) ===
    GtkWidget *text_frame = gtk_frame_new("Área de Texto Principal");
    text_view = gtk_text_view_new();
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    
    GtkWidget *text_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(text_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(text_scroll), text_view);
    gtk_container_add(GTK_CONTAINER(text_frame), text_scroll);
    
    // === ÁREA DE LOGS (INFERIOR) ===
    GtkWidget *log_frame = gtk_frame_new("Logs do Sistema");
    log_text_view = gtk_text_view_new();
    log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(log_text_view), GTK_WRAP_WORD);
    
    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(log_scroll), log_text_view);
    gtk_container_add(GTK_CONTAINER(log_frame), log_scroll);
    
    // Define altura mínima para os componentes
    gtk_widget_set_size_request(text_scroll, -1, 200);
    gtk_widget_set_size_request(log_scroll, -1, 100);
    
    // Adiciona tudo ao container principal
    gtk_box_pack_start(GTK_BOX(main_vbox), button_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), text_frame, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), log_frame, FALSE, TRUE, 5);
    
    // Conecta sinais
    g_signal_connect(main_window, "destroy", G_CALLBACK(on_main_window_destroy), NULL);
    g_signal_connect(alter_text_button, "clicked", G_CALLBACK(on_alter_text_clicked), NULL);
    g_signal_connect(chat_button, "clicked", G_CALLBACK(on_chat_clicked), NULL);
    
    // Mostra todos os widgets
    gtk_widget_show_all(main_window);
    
    // Adiciona log inicial
    add_log("Aplicação iniciada");
    char init_msg[256];
    snprintf(init_msg, sizeof(init_msg), "Processo %d de %d processos", my_rank, num_procs);
    add_log(init_msg);
    
    // Inicia o loop principal do GTK
    gtk_main();
    
    // Finaliza MPI
    MPI_Finalize();
    
    return 0;
}