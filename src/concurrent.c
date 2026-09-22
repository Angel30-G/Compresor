#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <locale.h>

#include "md5_util.h"
#include "huffman.h"

#define MAX_ARCHIVOS 200

typedef struct {
    char ruta_entrada[PATH_MAX];
    char ruta_salida[PATH_MAX];
    char nombre[256];
    long long tamano_original;
} TareaArchivo;

typedef struct {
    int exitoso;
    int verificado;
    long long tamano_original;
    long long tamano_comprimido;
} Resultado;

typedef struct {
    TareaArchivo *tarea;
    Resultado *resultado;
    pthread_mutex_t *mutex;
    int es_compresion;
} DatosHilo;

static double ahora(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int crear_directorio(const char *ruta) {
    struct stat st;

    if (stat(ruta, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        return -1;
    }

    if (mkdir(ruta, 0755) != 0) {
        perror("mkdir");
        return -1;
    }

    return 0;
}

static int cargar_archivos(
    const char *directorio_entrada,
    const char *directorio_salida,
    TareaArchivo *tareas
) {
    DIR *directorio = opendir(directorio_entrada);

    if (!directorio) {
        perror("opendir");
        return -1;
    }

    struct dirent *entrada;
    struct stat st;

    int cantidad = 0;

    while ((entrada = readdir(directorio)) != NULL) {
        if (entrada->d_name[0] == '.') {
            continue;
        }

        if (cantidad >= MAX_ARCHIVOS) {
            break;
        }

        snprintf(
            tareas[cantidad].ruta_entrada,
            sizeof(tareas[cantidad].ruta_entrada),
            "%s/%s",
            directorio_entrada,
            entrada->d_name
        );

        if (stat(tareas[cantidad].ruta_entrada, &st) != 0) {
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        snprintf(
            tareas[cantidad].ruta_salida,
            sizeof(tareas[cantidad].ruta_salida),
            "%s/%s.huff",
            directorio_salida,
            entrada->d_name
        );

        snprintf(
            tareas[cantidad].nombre,
            sizeof(tareas[cantidad].nombre),
            "%s",
            entrada->d_name
        );

        tareas[cantidad].tamano_original = st.st_size;

        cantidad++;
    }

    closedir(directorio);

    return cantidad;
}

static int cargar_archivos_comprimidos(
    const char *directorio_entrada,
    const char *directorio_salida,
    TareaArchivo *tareas
) {
    DIR *directorio = opendir(directorio_entrada);

    if (!directorio) {
        perror("opendir");
        return -1;
    }

    struct dirent *entrada;
    struct stat st;

    int cantidad = 0;

    while ((entrada = readdir(directorio)) != NULL) {
        if (entrada->d_name[0] == '.') {
            continue;
        }

        size_t largo = strlen(entrada->d_name);

        if (largo < 5) {
            continue;
        }

        if (
            strcmp(
                entrada->d_name + largo - 5,
                ".huff"
            ) != 0
        ) {
            continue;
        }

        if (cantidad >= MAX_ARCHIVOS) {
            break;
        }

        snprintf(
            tareas[cantidad].ruta_entrada,
            sizeof(tareas[cantidad].ruta_entrada),
            "%s/%s",
            directorio_entrada,
            entrada->d_name
        );

        if (stat(tareas[cantidad].ruta_entrada, &st) != 0) {
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        char nombre_base[256];

        strncpy(
            nombre_base,
            entrada->d_name,
            largo - 5
        );

        nombre_base[largo - 5] = '\0';

        snprintf(
            tareas[cantidad].ruta_salida,
            sizeof(tareas[cantidad].ruta_salida),
            "%s/%s",
            directorio_salida,
            nombre_base
        );

        snprintf(
            tareas[cantidad].nombre,
            sizeof(tareas[cantidad].nombre),
            "%s",
            nombre_base
        );

        tareas[cantidad].tamano_original = 0;

        cantidad++;
    }

    closedir(directorio);

    return cantidad;
}

static void *procesar_archivo(void *arg) {
    DatosHilo *datos = (DatosHilo *)arg;

    if (datos->es_compresion) {
        unsigned char md5[MD5_DIGEST_LENGTH];
        long long tamano_comprimido = 0;

        datos->resultado->exitoso = 0;
        datos->resultado->verificado = 0;
        datos->resultado->tamano_original =
            datos->tarea->tamano_original;
        datos->resultado->tamano_comprimido = 0;

        if (
            compute_md5(
                datos->tarea->ruta_entrada,
                md5
            ) == 0
        ) {
            if (
                compress_file(
                    datos->tarea->ruta_entrada,
                    datos->tarea->ruta_salida,
                    md5,
                    &tamano_comprimido
                ) == 0
            ) {
                pthread_mutex_lock(datos->mutex);

                datos->resultado->exitoso = 1;
                datos->resultado->verificado = 1;
                datos->resultado->tamano_comprimido =
                    tamano_comprimido;

                pthread_mutex_unlock(datos->mutex);
            }
        }
    } else {
        unsigned char md5_guardado[MD5_DIGEST_LENGTH];
        unsigned char md5_actual[MD5_DIGEST_LENGTH];

        datos->resultado->exitoso = 0;
        datos->resultado->verificado = 0;

        if (
            decompress_file(
                datos->tarea->ruta_entrada,
                datos->tarea->ruta_salida,
                md5_guardado
            ) == 0
        ) {
            if (
                compute_md5(
                    datos->tarea->ruta_salida,
                    md5_actual
                ) == 0
            ) {
                pthread_mutex_lock(datos->mutex);

                datos->resultado->exitoso = 1;

                if (
                    memcmp(
                        md5_guardado,
                        md5_actual,
                        MD5_DIGEST_LENGTH
                    ) == 0
                ) {
                    datos->resultado->verificado = 1;
                }

                pthread_mutex_unlock(datos->mutex);
            }
        }
    }

    return NULL;
}

static int ejecutar_hilos(
    TareaArchivo *tareas,
    Resultado *resultados,
    int cantidad,
    int es_compresion
) {
    pthread_mutex_t mutex;

    pthread_mutex_init(&mutex, NULL);

    long max_hilos = sysconf(_SC_NPROCESSORS_ONLN);

    if (max_hilos < 1) {
        max_hilos = 1;
    }

    pthread_t hilos[MAX_ARCHIVOS];
    DatosHilo datos[MAX_ARCHIVOS];

    int inicio = 0;

    while (inicio < cantidad) {
        int cantidad_actual = 0;

        for (
            int i = 0;
            i < max_hilos && inicio + i < cantidad;
            i++
        ) {
            int posicion = inicio + i;

            datos[posicion].tarea = &tareas[posicion];
            datos[posicion].resultado = &resultados[posicion];
            datos[posicion].mutex = &mutex;
            datos[posicion].es_compresion = es_compresion;

            if (
                pthread_create(
                    &hilos[posicion],
                    NULL,
                    procesar_archivo,
                    &datos[posicion]
                ) == 0
            ) {
                cantidad_actual++;
            }
        }

        for (int i = 0; i < cantidad_actual; i++) {
            pthread_join(
                hilos[inicio + i],
                NULL
            );
        }

        inicio += cantidad_actual;

        if (cantidad_actual == 0) {
            break;
        }
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}

static int comprimir_directorio_concurrente(
    const char *directorio_entrada,
    const char *directorio_salida
) {
    double tiempo_inicio = ahora();

    if (crear_directorio(directorio_salida) != 0) {
        fprintf(
            stderr,
            "No se pudo crear el directorio de salida\n"
        );

        return 1;
    }

    TareaArchivo tareas[MAX_ARCHIVOS];
    Resultado resultados[MAX_ARCHIVOS];

    memset(
        resultados,
        0,
        sizeof(resultados)
    );

    int cantidad = cargar_archivos(
        directorio_entrada,
        directorio_salida,
        tareas
    );

    if (cantidad < 0) {
        return 1;
    }

    if (cantidad == 0) {
        printf("No se encontraron archivos\n");
        return 0;
    }

    ejecutar_hilos(
        tareas,
        resultados,
        cantidad,
        1
    );

    int procesados = 0;
    int verificados = 0;

    long long total_original = 0;
    long long total_comprimido = 0;

    for (int i = 0; i < cantidad; i++) {
        if (resultados[i].exitoso) {
            printf(
                "[OK] %s | orig=%lld comp=%lld\n",
                tareas[i].nombre,
                resultados[i].tamano_original,
                resultados[i].tamano_comprimido
            );

            procesados++;

            if (resultados[i].verificado) {
                verificados++;
            }

            total_original +=
                resultados[i].tamano_original;

            total_comprimido +=
                resultados[i].tamano_comprimido;
        } else {
            printf(
                "[FALLO] %s\n",
                tareas[i].nombre
            );
        }
    }

    double tiempo_fin = ahora();

    printf(
        "TIEMPO_COMP=%.6f\n",
        tiempo_fin - tiempo_inicio
    );

    printf("TIEMPO_DESCOMP=0.000000\n");
    printf("ARCHIVOS=%d\n", procesados);
    printf("VERIFICADOS=%d\n", verificados);
    printf("BYTES_ORIG=%lld\n", total_original);
    printf("BYTES_COMP=%lld\n", total_comprimido);

    printf(
        "RADIO=%.6f\n",
        total_original > 0
            ? (double)total_comprimido / total_original
            : 0.0
    );

    return 0;
}

static int descomprimir_directorio_concurrente(
    const char *directorio_entrada,
    const char *directorio_salida
) {
    double tiempo_inicio = ahora();

    if (crear_directorio(directorio_salida) != 0) {
        fprintf(
            stderr,
            "No se pudo crear el directorio de salida\n"
        );

        return 1;
    }

    TareaArchivo tareas[MAX_ARCHIVOS];
    Resultado resultados[MAX_ARCHIVOS];

    memset(
        resultados,
        0,
        sizeof(resultados)
    );

    int cantidad = cargar_archivos_comprimidos(
        directorio_entrada,
        directorio_salida,
        tareas
    );

    if (cantidad < 0) {
        return 1;
    }

    if (cantidad == 0) {
        printf("No se encontraron archivos comprimidos\n");
        return 0;
    }

    ejecutar_hilos(
        tareas,
        resultados,
        cantidad,
        0
    );

    int procesados = 0;
    int verificados = 0;

    for (int i = 0; i < cantidad; i++) {

        if (resultados[i].exitoso) {
            procesados++;

            if (resultados[i].verificado) {
                printf(
                    "[OK] %s | MD5 verificado\n",
                    tareas[i].nombre
                );

                verificados++;
            } else {
                printf(
                    "[FALLO] %s | MD5 no coincide\n",
                    tareas[i].nombre
                );
            }
        } else {
            printf(
                "[FALLO] %s | error de descompresion\n",
                tareas[i].nombre
            );
        }
    }

    double tiempo_fin = ahora();

    printf("TIEMPO_COMP=0.000000\n");

    printf(
        "TIEMPO_DESCOMP=%.6f\n",
        tiempo_fin - tiempo_inicio
    );

    printf("ARCHIVOS=%d\n", procesados);
    printf("VERIFICADOS=%d\n", verificados);
    printf("BYTES_ORIG=0\n");
    printf("BYTES_COMP=0\n");
    printf("RADIO=0.000000\n");

    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");

    if (argc != 4) {
        fprintf(stderr, "Uso:\n");

        fprintf(
            stderr,
            "  %s c <dir_entrada> <dir_salida>\n",
            argv[0]
        );

        fprintf(
            stderr,
            "  %s d <dir_entrada> <dir_salida>\n",
            argv[0]
        );

        return 1;
    }

    if (strcmp(argv[1], "c") == 0) {
        return comprimir_directorio_concurrente(
            argv[2],
            argv[3]
        );
    }

    if (strcmp(argv[1], "d") == 0) {
        return descomprimir_directorio_concurrente(
            argv[2],
            argv[3]
        );
    }

    fprintf(
        stderr,
        "Modo desconocido: %s\n",
        argv[1]
    );

    return 1;
}
