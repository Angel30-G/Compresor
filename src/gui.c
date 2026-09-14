#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

/* ==================== Estructuras ==================== */
typedef struct {
    double tiempo_compresion;
    double tiempo_descompresion;
    int archivos_totales;
    int archivos_verificados;
    long long bytes_originales;
    long long bytes_comprimidos;
    double aceleracion_compresion;
    double aceleracion_descompresion;
    double radio_compresion;
} Stats;

/* ==================== Variables globales ==================== */
static gchar *directorio_seleccionado = NULL;
static GtkListStore *store_estadisticas = NULL;

enum {
    COL_MODO = 0,
    COL_SALUD,
    COL_T_COMP,
    COL_T_DESCOMP,
    COL_ACEL_COMP,
    COL_ACEL_DESCOMP,
    COL_BYTES_ORIG,
    COL_BYTES_COMP,
    COL_RADIO,
    NUM_COLUMNAS
};

/* ==================== Prototipos ==================== */
static void on_seleccionar_directorio(GtkWidget *widget, gpointer data);
static void on_comprimir_serial(GtkWidget *widget, gpointer data);
static void on_comprimir_parallel(GtkWidget *widget, gpointer data);
static void on_comprimir_concurrent(GtkWidget *widget, gpointer data);
static void on_descomprimir_serial(GtkWidget *widget, gpointer data);
static void on_descomprimir_parallel(GtkWidget *widget, gpointer data);
static void on_descomprimir_concurrent(GtkWidget *widget, gpointer data);
static void agregar_fila_stats(const char *modo, Stats *stats);
static gboolean ejecutar_comando(const char *comando, char *salida, size_t max_len);
static void parsear_stats(const char *salida, Stats *stats);

/* ==================== Ejecutar comando y capturar salida ==================== */
static gboolean ejecutar_comando(const char *comando, char *salida, size_t max_len) {
    FILE *fp = popen(comando, "r");
    if (!fp) return FALSE;

    size_t total = 0;
    size_t n;
    while ((n = fread(salida + total, 1, max_len - total - 1, fp)) > 0) {
        total += n;
        if (total >= max_len - 1) break;
    }
    salida[total] = '\0';

    int status = pclose(fp);
    return (status == 0);
}

/* ==================== Parsear salida ==================== */
static void parsear_stats(const char *salida, Stats *stats) {
    char *copia = strdup(salida);
    char *linea = strtok(copia, "\n");
    while (linea) {
        if (sscanf(linea, "TIEMPO_COMP=%lf", &stats->tiempo_compresion) == 1) {}
        else if (sscanf(linea, "TIEMPO_DESCOMP=%lf", &stats->tiempo_descompresion) == 1) {}
        else if (sscanf(linea, "ARCHIVOS=%d", &stats->archivos_totales) == 1) {}
        else if (sscanf(linea, "VERIFICADOS=%d", &stats->archivos_verificados) == 1) {}
        else if (sscanf(linea, "BYTES_ORIG=%lld", &stats->bytes_originales) == 1) {}
        else if (sscanf(linea, "BYTES_COMP=%lld", &stats->bytes_comprimidos) == 1) {}
        else if (sscanf(linea, "RADIO=%lf", &stats->radio_compresion) == 1) {}
        linea = strtok(NULL, "\n");
    }
    free(copia);
}

/* ==================== Agregar fila a la tabla ==================== */
static void agregar_fila_stats(const char *modo, Stats *stats) {
    GtkTreeIter iter;
    gtk_list_store_append(store_estadisticas, &iter);

    char buf_salud[32];
    snprintf(buf_salud, sizeof(buf_salud), "%d/%d (%.1f%%)",
             stats->archivos_verificados, stats->archivos_totales,
             stats->archivos_totales > 0 ? 100.0 * stats->archivos_verificados / stats->archivos_totales : 0.0);

    char buf_tcomp[32];
    snprintf(buf_tcomp, sizeof(buf_tcomp), "%.2f s", stats->tiempo_compresion);

    char buf_tdescomp[32];
    snprintf(buf_tdescomp, sizeof(buf_tdescomp), "%.2f s", stats->tiempo_descompresion);

    char buf_acel_comp[32];
    snprintf(buf_acel_comp, sizeof(buf_acel_comp), "%.1f%%", stats->aceleracion_compresion);

    char buf_acel_descomp[32];
    snprintf(buf_acel_descomp, sizeof(buf_acel_descomp), "%.1f%%", stats->aceleracion_descompresion);

    char buf_borig[32];
    snprintf(buf_borig, sizeof(buf_borig), "%lld", stats->bytes_originales);

    char buf_bcomp[32];
    snprintf(buf_bcomp, sizeof(buf_bcomp), "%lld", stats->bytes_comprimidos);

    char buf_radio[32];
    snprintf(buf_radio, sizeof(buf_radio), "%.4f", stats->radio_compresion);

    gtk_list_store_set(store_estadisticas, &iter,
                       COL_MODO, modo,
                       COL_SALUD, buf_salud,
                       COL_T_COMP, buf_tcomp,
                       COL_T_DESCOMP, buf_tdescomp,
                       COL_ACEL_COMP, buf_acel_comp,
                       COL_ACEL_DESCOMP, buf_acel_descomp,
                       COL_BYTES_ORIG, buf_borig,
                       COL_BYTES_COMP, buf_bcomp,
                       COL_RADIO, buf_radio,
                       -1);
}

/* ==================== Callbacks ==================== */
static void on_seleccionar_directorio(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Seleccionar directorio",
                                                     GTK_WINDOW(data),
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                     "_Cancelar", GTK_RESPONSE_CANCEL,
                                                     "_Abrir", GTK_RESPONSE_ACCEPT,
                                                     NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        if (directorio_seleccionado) g_free(directorio_seleccionado);
        directorio_seleccionado = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_print("Directorio seleccionado: %s\n", directorio_seleccionado);
    }
    gtk_widget_destroy(dialog);
}

static void on_comprimir_serial(GtkWidget *widget, gpointer data) {
    if (!directorio_seleccionado) { g_print("Seleccione un directorio primero.\n"); return; }
    char comando[2048];
    snprintf(comando, sizeof(comando), "bin/compresor_serial c \"%s\" data/comprimidos", directorio_seleccionado);
    char salida[8192] = {0};
    if (ejecutar_comando(comando, salida, sizeof(salida))) {
        Stats stats = {0};
        parsear_stats(salida, &stats);
        agregar_fila_stats("Serial", &stats);
        g_print("Compresión serial completada.\n");
    } else {
        g_print("Error al ejecutar compresor serial.\n");
    }
}

static void on_comprimir_parallel(GtkWidget *widget, gpointer data) {
    if (!directorio_seleccionado) { g_print("Seleccione un directorio primero.\n"); return; }
    char comando[2048];
    snprintf(comando, sizeof(comando), "bin/compresor_parallel \"%s\" data/comprimidos_par", directorio_seleccionado);
    char salida[8192] = {0};
    if (ejecutar_comando(comando, salida, sizeof(salida))) {
        Stats stats = {0};
        parsear_stats(salida, &stats);
        agregar_fila_stats("Paralelo", &stats);
        g_print("Compresión paralela completada.\n");
    } else {
        g_print("Error al ejecutar compresor paralelo.\n");
    }
}

static void on_comprimir_concurrent(GtkWidget *widget, gpointer data) {
    if (!directorio_seleccionado) { g_print("Seleccione un directorio primero.\n"); return; }
    char comando[2048];
    snprintf(comando, sizeof(comando), "bin/compresor_concurrent \"%s\" data/comprimidos_conc", directorio_seleccionado);
    char salida[8192] = {0};
    if (ejecutar_comando(comando, salida, sizeof(salida))) {
        Stats stats = {0};
        parsear_stats(salida, &stats);
        agregar_fila_stats("Concurrente", &stats);
        g_print("Compresión concurrente completada.\n");
    } else {
        g_print("Error al ejecutar compresor concurrente.\n");
    }
}

static void on_descomprimir_serial(GtkWidget *widget, gpointer data) {
    char comando[2048];
    snprintf(comando, sizeof(comando), "bin/compresor_serial d data/comprimidos data/descomprimidos");
    char salida[8192] = {0};
    if (ejecutar_comando(comando, salida, sizeof(salida))) {
        Stats stats = {0};
        parsear_stats(salida, &stats);
        agregar_fila_stats("Descompresión Serial", &stats);
        g_print("Descompresión serial completada.\n");
    } else {
        g_print("Error al ejecutar descompresor serial.\n");
    }
}

static void on_descomprimir_parallel(GtkWidget *widget, gpointer data) {
    char comando[2048];
    snprintf(comando, sizeof(comando), "bin/descompresor_parallel data/comprimidos_par /tmp/salida_descomprimida_par");
    char salida[8192] = {0};
    if (ejecutar_comando(comando, salida, sizeof(salida))) {
        Stats stats = {0};
        parsear_stats(salida, &stats);
        agregar_fila_stats("Descompresión Paralela", &stats);
        g_print("Descompresión paralela completada.\n");
    } else {
        g_print("Error al ejecutar descompresor paralelo.\n");
    }
}

static void on_descomprimir_concurrent(GtkWidget *widget, gpointer data) {
    char comando[2048];
    snprintf(comando, sizeof(comando), "bin/descompresor_concurrent data/comprimidos_conc /tmp/salida_descomprimida_conc");
    char salida[8192] = {0};
    if (ejecutar_comando(comando, salida, sizeof(salida))) {
        Stats stats = {0};
        parsear_stats(salida, &stats);
        agregar_fila_stats("Descompresión Concurrente", &stats);
        g_print("Descompresión concurrente completada.\n");
    } else {
        g_print("Error al ejecutar descompresor concurrente.\n");
    }
}

/* ==================== Construir interfaz ==================== */
static void construir_interfaz(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Compresor Huffman - Estadísticas");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 600);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Botón seleccionar directorio */
    GtkWidget *hbox_dir = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *btn_dir = gtk_button_new_with_label("Seleccionar directorio...");
    g_signal_connect(btn_dir, "clicked", G_CALLBACK(on_seleccionar_directorio), window);
    gtk_box_pack_start(GTK_BOX(hbox_dir), btn_dir, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_dir, FALSE, FALSE, 5);

    /* Botones compresión */
    GtkWidget *hbox_comp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *btn_cs = gtk_button_new_with_label("Comprimir Serial");
    GtkWidget *btn_cp = gtk_button_new_with_label("Comprimir Paralelo");
    GtkWidget *btn_cc = gtk_button_new_with_label("Comprimir Concurrente");
    g_signal_connect(btn_cs, "clicked", G_CALLBACK(on_comprimir_serial), NULL);
    g_signal_connect(btn_cp, "clicked", G_CALLBACK(on_comprimir_parallel), NULL);
    g_signal_connect(btn_cc, "clicked", G_CALLBACK(on_comprimir_concurrent), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_comp), btn_cs, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox_comp), btn_cp, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox_comp), btn_cc, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_comp, FALSE, FALSE, 5);

    /* Botones descompresión */
    GtkWidget *hbox_descomp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *btn_ds = gtk_button_new_with_label("Descomprimir Serial");
    GtkWidget *btn_dp = gtk_button_new_with_label("Descomprimir Paralelo");
    GtkWidget *btn_dc = gtk_button_new_with_label("Descomprimir Concurrente");
    g_signal_connect(btn_ds, "clicked", G_CALLBACK(on_descomprimir_serial), NULL);
    g_signal_connect(btn_dp, "clicked", G_CALLBACK(on_descomprimir_parallel), NULL);
    g_signal_connect(btn_dc, "clicked", G_CALLBACK(on_descomprimir_concurrent), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_descomp), btn_ds, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox_descomp), btn_dp, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox_descomp), btn_dc, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_descomp, FALSE, FALSE, 5);

    /* Tabla */
    store_estadisticas = gtk_list_store_new(NUM_COLUMNAS,
                                            G_TYPE_STRING, G_TYPE_STRING,
                                            G_TYPE_STRING, G_TYPE_STRING,
                                            G_TYPE_STRING, G_TYPE_STRING,
                                            G_TYPE_STRING, G_TYPE_STRING,
                                            G_TYPE_STRING);

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_estadisticas));

    const char *titulos[] = {
        "Modo", "Salud", "T. Compresión", "T. Descompresión",
        "Acel. Compresión", "Acel. Descompresión",
        "Bytes Originales", "Bytes Comprimidos", "Radio"
    };

    for (int i = 0; i < NUM_COLUMNAS; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *columna = gtk_tree_view_column_new_with_attributes(
            titulos[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), columna);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), treeview);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    gtk_widget_show_all(window);
}

/* ==================== main ==================== */
int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");
    GtkApplication *app = gtk_application_new("org.ejemplo.compresor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(construir_interfaz), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
