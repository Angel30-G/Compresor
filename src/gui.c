#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

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

static gchar *directorio_seleccionado = NULL;
static GtkListStore *store_estadisticas = NULL;

static double tiempo_serial_compresion = 0.0;
static double tiempo_serial_descompresion = 0.0;

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

static gboolean ejecutar_comando(
    const char *comando,
    char *salida,
    size_t max_len
) {
    FILE *fp = popen(comando, "r");

    if (!fp) {
        return FALSE;
    }

    size_t total = 0;
    size_t n;

    while (
        (n = fread(
            salida + total,
            1,
            max_len - total - 1,
            fp
        )) > 0
    ) {
        total += n;

        if (total >= max_len - 1) {
            break;
        }
    }

    salida[total] = '\0';

    int estado = pclose(fp);

    return estado == 0;
}

static void parsear_stats(
    const char *salida,
    Stats *stats
) {
    char *copia = strdup(salida);
    char *linea = strtok(copia, "\n");

    while (linea) {
        if (
            sscanf(
                linea,
                "TIEMPO_COMP=%lf",
                &stats->tiempo_compresion
            ) == 1
        ) {
        } else if (
            sscanf(
                linea,
                "TIEMPO_DESCOMP=%lf",
                &stats->tiempo_descompresion
            ) == 1
        ) {
        } else if (
            sscanf(
                linea,
                "ARCHIVOS=%d",
                &stats->archivos_totales
            ) == 1
        ) {
        } else if (
            sscanf(
                linea,
                "VERIFICADOS=%d",
                &stats->archivos_verificados
            ) == 1
        ) {
        } else if (
            sscanf(
                linea,
                "BYTES_ORIG=%lld",
                &stats->bytes_originales
            ) == 1
        ) {
        } else if (
            sscanf(
                linea,
                "BYTES_COMP=%lld",
                &stats->bytes_comprimidos
            ) == 1
        ) {
        } else if (
            sscanf(
                linea,
                "RADIO=%lf",
                &stats->radio_compresion
            ) == 1
        ) {
        }

        linea = strtok(NULL, "\n");
    }

    free(copia);
}

static void agregar_fila_stats(
    const char *modo,
    Stats *stats
) {
    GtkTreeIter iter;

    gtk_list_store_append(
        store_estadisticas,
        &iter
    );

    char salud[32];
    char tiempo_compresion[32];
    char tiempo_descompresion[32];
    char aceleracion_compresion[32];
    char aceleracion_descompresion[32];
    char bytes_originales[32];
    char bytes_comprimidos[32];
    char radio[32];

    double porcentaje_salud = 0.0;

    if (stats->archivos_totales > 0) {
        porcentaje_salud =
            100.0 *
            stats->archivos_verificados /
            stats->archivos_totales;
    }

    snprintf(
        salud,
        sizeof(salud),
        "%d/%d (%.1f%%)",
        stats->archivos_verificados,
        stats->archivos_totales,
        porcentaje_salud
    );

    snprintf(
        tiempo_compresion,
        sizeof(tiempo_compresion),
        "%.6f s",
        stats->tiempo_compresion
    );

    snprintf(
        tiempo_descompresion,
        sizeof(tiempo_descompresion),
        "%.6f s",
        stats->tiempo_descompresion
    );

    snprintf(
        aceleracion_compresion,
        sizeof(aceleracion_compresion),
        "%.1f%%",
        stats->aceleracion_compresion
    );

    snprintf(
        aceleracion_descompresion,
        sizeof(aceleracion_descompresion),
        "%.1f%%",
        stats->aceleracion_descompresion
    );

    snprintf(
        bytes_originales,
        sizeof(bytes_originales),
        "%lld",
        stats->bytes_originales
    );

    snprintf(
        bytes_comprimidos,
        sizeof(bytes_comprimidos),
        "%lld",
        stats->bytes_comprimidos
    );

    snprintf(
        radio,
        sizeof(radio),
        "%.4f",
        stats->radio_compresion
    );

    gtk_list_store_set(
        store_estadisticas,
        &iter,
        COL_MODO, modo,
        COL_SALUD, salud,
        COL_T_COMP, tiempo_compresion,
        COL_T_DESCOMP, tiempo_descompresion,
        COL_ACEL_COMP, aceleracion_compresion,
        COL_ACEL_DESCOMP, aceleracion_descompresion,
        COL_BYTES_ORIG, bytes_originales,
        COL_BYTES_COMP, bytes_comprimidos,
        COL_RADIO, radio,
        -1
    );
}

static void on_seleccionar_directorio(
    GtkWidget *widget,
    gpointer data
) {
    GtkWidget *dialogo =
        gtk_file_chooser_dialog_new(
            "Seleccionar directorio",
            GTK_WINDOW(data),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "_Cancelar",
            GTK_RESPONSE_CANCEL,
            "_Abrir",
            GTK_RESPONSE_ACCEPT,
            NULL
        );

    if (
        gtk_dialog_run(
            GTK_DIALOG(dialogo)
        ) == GTK_RESPONSE_ACCEPT
    ) {
        if (directorio_seleccionado) {
            g_free(directorio_seleccionado);
        }

        directorio_seleccionado =
            gtk_file_chooser_get_filename(
                GTK_FILE_CHOOSER(dialogo)
            );

        g_print(
            "Directorio seleccionado: %s\n",
            directorio_seleccionado
        );
    }

    gtk_widget_destroy(dialogo);
}

static void on_comprimir_serial(
    GtkWidget *widget,
    gpointer data
) {
    if (!directorio_seleccionado) {
        g_print(
            "Seleccione un directorio primero.\n"
        );
        return;
    }

    char comando[2048];

    snprintf(
        comando,
        sizeof(comando),
        "bin/compresor_serial c \"%s\" data/comprimidos",
        directorio_seleccionado
    );

    char salida[8192] = {0};

    if (
        ejecutar_comando(
            comando,
            salida,
            sizeof(salida)
        )
    ) {
        Stats stats = {0};

        parsear_stats(
            salida,
            &stats
        );

        tiempo_serial_compresion =
            stats.tiempo_compresion;

        stats.aceleracion_compresion = 0.0;

        agregar_fila_stats(
            "Serial",
            &stats
        );

        g_print(
            "Compresion serial completada.\n"
        );
    } else {
        g_print(
            "Error al ejecutar compresor serial.\n"
        );
    }
}

static void on_comprimir_parallel(
    GtkWidget *widget,
    gpointer data
) {
    if (!directorio_seleccionado) {
        g_print(
            "Seleccione un directorio primero.\n"
        );
        return;
    }

    char comando[2048];

    snprintf(
        comando,
        sizeof(comando),
        "bin/compresor_parallel c \"%s\" data/comprimidos_par",
        directorio_seleccionado
    );

    char salida[8192] = {0};

    if (
        ejecutar_comando(
            comando,
            salida,
            sizeof(salida)
        )
    ) {
        Stats stats = {0};

        parsear_stats(
            salida,
            &stats
        );

        if (
            tiempo_serial_compresion > 0.0 &&
            stats.tiempo_compresion > 0.0
        ) {
            stats.aceleracion_compresion =
                (
                    (
                        tiempo_serial_compresion -
                        stats.tiempo_compresion
                    ) /
                    tiempo_serial_compresion
                ) * 100.0;
        }

        agregar_fila_stats(
            "Paralelo",
            &stats
        );

        g_print(
            "Compresion paralela completada.\n"
        );
    } else {
        g_print(
            "Error al ejecutar compresor paralelo.\n"
        );
    }
}

static void on_comprimir_concurrent(
    GtkWidget *widget,
    gpointer data
) {
    if (!directorio_seleccionado) {
        g_print(
            "Seleccione un directorio primero.\n"
        );
        return;
    }

    char comando[2048];

    snprintf(
        comando,
        sizeof(comando),
        "bin/compresor_concurrent c \"%s\" data/comprimidos_conc",
        directorio_seleccionado
    );

    char salida[8192] = {0};

    if (
        ejecutar_comando(
            comando,
            salida,
            sizeof(salida)
        )
    ) {
        Stats stats = {0};

        parsear_stats(
            salida,
            &stats
        );

        if (
            tiempo_serial_compresion > 0.0 &&
            stats.tiempo_compresion > 0.0
        ) {
            stats.aceleracion_compresion =
                (
                    (
                        tiempo_serial_compresion -
                        stats.tiempo_compresion
                    ) /
                    tiempo_serial_compresion
                ) * 100.0;
        }

        agregar_fila_stats(
            "Concurrente",
            &stats
        );

        g_print(
            "Compresion concurrente completada.\n"
        );
    } else {
        g_print(
            "Error al ejecutar compresor concurrente.\n"
        );
    }
}

static void on_descomprimir_serial(
    GtkWidget *widget,
    gpointer data
) {
    char comando[2048];

    snprintf(
        comando,
        sizeof(comando),
        "bin/compresor_serial d data/comprimidos data/descomprimidos"
    );

    char salida[8192] = {0};

    if (
        ejecutar_comando(
            comando,
            salida,
            sizeof(salida)
        )
    ) {
        Stats stats = {0};

        parsear_stats(
            salida,
            &stats
        );

        tiempo_serial_descompresion =
            stats.tiempo_descompresion;

        stats.aceleracion_descompresion = 0.0;

        agregar_fila_stats(
            "Descompresion Serial",
            &stats
        );

        g_print(
            "Descompresion serial completada.\n"
        );
    } else {
        g_print(
            "Error al ejecutar descompresor serial.\n"
        );
    }
}

static void on_descomprimir_parallel(
    GtkWidget *widget,
    gpointer data
) {
    char comando[2048];

    snprintf(
        comando,
        sizeof(comando),
        "bin/compresor_parallel d data/comprimidos_par data/descomprimidos_par"
    );

    char salida[8192] = {0};

    if (
        ejecutar_comando(
            comando,
            salida,
            sizeof(salida)
        )
    ) {
        Stats stats = {0};

        parsear_stats(
            salida,
            &stats
        );

        if (
            tiempo_serial_descompresion > 0.0 &&
            stats.tiempo_descompresion > 0.0
        ) {
            stats.aceleracion_descompresion =
                (
                    (
                        tiempo_serial_descompresion -
                        stats.tiempo_descompresion
                    ) /
                    tiempo_serial_descompresion
                ) * 100.0;
        }

        agregar_fila_stats(
            "Descompresion Paralela",
            &stats
        );

        g_print(
            "Descompresion paralela completada.\n"
        );
    } else {
        g_print(
            "Error al ejecutar descompresor paralelo.\n"
        );
    }
}

static void on_descomprimir_concurrent(
    GtkWidget *widget,
    gpointer data
) {
    char comando[2048];

    snprintf(
        comando,
        sizeof(comando),
        "bin/compresor_concurrent d data/comprimidos_conc data/descomprimidos_conc"
    );

    char salida[8192] = {0};

    if (
        ejecutar_comando(
            comando,
            salida,
            sizeof(salida)
        )
    ) {
        Stats stats = {0};

        parsear_stats(
            salida,
            &stats
        );

        if (
            tiempo_serial_descompresion > 0.0 &&
            stats.tiempo_descompresion > 0.0
        ) {
            stats.aceleracion_descompresion =
                (
                    (
                        tiempo_serial_descompresion -
                        stats.tiempo_descompresion
                    ) /
                    tiempo_serial_descompresion
                ) * 100.0;
        }

        agregar_fila_stats(
            "Descompresion Concurrente",
            &stats
        );

        g_print(
            "Descompresion concurrente completada.\n"
        );
    } else {
        g_print(
            "Error al ejecutar descompresor concurrente.\n"
        );
    }
}

static void construir_interfaz(
    GtkApplication *app,
    gpointer user_data
) {
    GtkWidget *ventana =
        gtk_application_window_new(app);

    gtk_window_set_title(
        GTK_WINDOW(ventana),
        "Compresor Huffman - Estadisticas"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(ventana),
        1200,
        600
    );

    GtkWidget *caja_principal =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            5
        );

    gtk_container_add(
        GTK_CONTAINER(ventana),
        caja_principal
    );

    GtkWidget *caja_directorio =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            5
        );

    GtkWidget *boton_directorio =
        gtk_button_new_with_label(
            "Seleccionar directorio..."
        );

    g_signal_connect(
        boton_directorio,
        "clicked",
        G_CALLBACK(on_seleccionar_directorio),
        ventana
    );

    gtk_box_pack_start(
        GTK_BOX(caja_directorio),
        boton_directorio,
        FALSE,
        FALSE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_principal),
        caja_directorio,
        FALSE,
        FALSE,
        5
    );

    GtkWidget *caja_compresion =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            5
        );

    GtkWidget *boton_comp_serial =
        gtk_button_new_with_label(
            "Comprimir Serial"
        );

    GtkWidget *boton_comp_paralelo =
        gtk_button_new_with_label(
            "Comprimir Paralelo"
        );

    GtkWidget *boton_comp_concurrente =
        gtk_button_new_with_label(
            "Comprimir Concurrente"
        );

    g_signal_connect(
        boton_comp_serial,
        "clicked",
        G_CALLBACK(on_comprimir_serial),
        NULL
    );

    g_signal_connect(
        boton_comp_paralelo,
        "clicked",
        G_CALLBACK(on_comprimir_parallel),
        NULL
    );

    g_signal_connect(
        boton_comp_concurrente,
        "clicked",
        G_CALLBACK(on_comprimir_concurrent),
        NULL
    );

    gtk_box_pack_start(
        GTK_BOX(caja_compresion),
        boton_comp_serial,
        TRUE,
        TRUE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_compresion),
        boton_comp_paralelo,
        TRUE,
        TRUE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_compresion),
        boton_comp_concurrente,
        TRUE,
        TRUE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_principal),
        caja_compresion,
        FALSE,
        FALSE,
        5
    );

    GtkWidget *caja_descompresion =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            5
        );

    GtkWidget *boton_descomp_serial =
        gtk_button_new_with_label(
            "Descomprimir Serial"
        );

    GtkWidget *boton_descomp_paralelo =
        gtk_button_new_with_label(
            "Descomprimir Paralelo"
        );

    GtkWidget *boton_descomp_concurrente =
        gtk_button_new_with_label(
            "Descomprimir Concurrente"
        );

    g_signal_connect(
        boton_descomp_serial,
        "clicked",
        G_CALLBACK(on_descomprimir_serial),
        NULL
    );

    g_signal_connect(
        boton_descomp_paralelo,
        "clicked",
        G_CALLBACK(on_descomprimir_parallel),
        NULL
    );

    g_signal_connect(
        boton_descomp_concurrente,
        "clicked",
        G_CALLBACK(on_descomprimir_concurrent),
        NULL
    );

    gtk_box_pack_start(
        GTK_BOX(caja_descompresion),
        boton_descomp_serial,
        TRUE,
        TRUE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_descompresion),
        boton_descomp_paralelo,
        TRUE,
        TRUE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_descompresion),
        boton_descomp_concurrente,
        TRUE,
        TRUE,
        5
    );

    gtk_box_pack_start(
        GTK_BOX(caja_principal),
        caja_descompresion,
        FALSE,
        FALSE,
        5
    );

    store_estadisticas =
        gtk_list_store_new(
            NUM_COLUMNAS,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING
        );

    GtkWidget *tabla =
        gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(
                store_estadisticas
            )
        );

    const char *titulos[] = {
        "Modo",
        "Salud",
        "T. Compresion",
        "T. Descompresion",
        "Acel. Compresion",
        "Acel. Descompresion",
        "Bytes Originales",
        "Bytes Comprimidos",
        "Radio"
    };

    for (int i = 0; i < NUM_COLUMNAS; i++) {
        GtkCellRenderer *renderer =
            gtk_cell_renderer_text_new();

        GtkTreeViewColumn *columna =
            gtk_tree_view_column_new_with_attributes(
                titulos[i],
                renderer,
                "text",
                i,
                NULL
            );

        gtk_tree_view_append_column(
            GTK_TREE_VIEW(tabla),
            columna
        );
    }

    GtkWidget *scroll =
        gtk_scrolled_window_new(
            NULL,
            NULL
        );

    gtk_container_add(
        GTK_CONTAINER(scroll),
        tabla
    );

    gtk_box_pack_start(
        GTK_BOX(caja_principal),
        scroll,
        TRUE,
        TRUE,
        5
    );

    gtk_widget_show_all(ventana);
}

int main(int argc, char *argv[]) {
    setlocale(
        LC_NUMERIC,
        "C"
    );

    GtkApplication *app =
        gtk_application_new(
            "org.ejemplo.compresor",
            G_APPLICATION_DEFAULT_FLAGS
        );

    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(construir_interfaz),
        NULL
    );

    int estado =
        g_application_run(
            G_APPLICATION(app),
            argc,
            argv
        );

    g_object_unref(app);

    return estado;
}
