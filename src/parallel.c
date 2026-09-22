#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
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

static int comprimir_directorio_paralelo(
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

    Resultado *resultados = mmap(
        NULL,
        sizeof(Resultado) * cantidad,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (resultados == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    memset(
        resultados,
        0,
        sizeof(Resultado) * cantidad
    );

    long max_procesos = sysconf(_SC_NPROCESSORS_ONLN);

    if (max_procesos < 1) {
        max_procesos = 1;
    }

    int procesos_activos = 0;

    for (int i = 0; i < cantidad; i++) {

        while (procesos_activos >= max_procesos) {
            wait(NULL);
            procesos_activos--;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            unsigned char md5[MD5_DIGEST_LENGTH];
            long long tamano_comprimido = 0;

            resultados[i].exitoso = 0;
            resultados[i].verificado = 0;
            resultados[i].tamano_original =
                tareas[i].tamano_original;
            resultados[i].tamano_comprimido = 0;

            if (
                compute_md5(
                    tareas[i].ruta_entrada,
                    md5
                ) == 0
            ) {
                if (
                    compress_file(
                        tareas[i].ruta_entrada,
                        tareas[i].ruta_salida,
                        md5,
                        &tamano_comprimido
                    ) == 0
                ) {
                    resultados[i].exitoso = 1;
                    resultados[i].verificado = 1;
                    resultados[i].tamano_comprimido =
                        tamano_comprimido;
                }
            }

            _exit(0);
        }

        procesos_activos++;
    }

    while (procesos_activos > 0) {
        wait(NULL);
        procesos_activos--;
    }

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

    printf(
        "TIEMPO_DESCOMP=0.000000\n"
    );

    printf(
        "ARCHIVOS=%d\n",
        procesados
    );

    printf(
        "VERIFICADOS=%d\n",
        verificados
    );

    printf(
        "BYTES_ORIG=%lld\n",
        total_original
    );

    printf(
        "BYTES_COMP=%lld\n",
        total_comprimido
    );

    printf(
        "RADIO=%.6f\n",
        total_original > 0
            ? (double)total_comprimido / total_original
            : 0.0
    );

    munmap(
        resultados,
        sizeof(Resultado) * cantidad
    );

    return 0;
}

static int descomprimir_directorio_paralelo(
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

    Resultado *resultados = mmap(
        NULL,
        sizeof(Resultado) * cantidad,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (resultados == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    memset(
        resultados,
        0,
        sizeof(Resultado) * cantidad
    );

    long max_procesos = sysconf(_SC_NPROCESSORS_ONLN);

    if (max_procesos < 1) {
        max_procesos = 1;
    }

    int procesos_activos = 0;

    for (int i = 0; i < cantidad; i++) {

        while (procesos_activos >= max_procesos) {
            wait(NULL);
            procesos_activos--;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            unsigned char md5_guardado[MD5_DIGEST_LENGTH];
            unsigned char md5_actual[MD5_DIGEST_LENGTH];

            resultados[i].exitoso = 0;
            resultados[i].verificado = 0;

            if (
                decompress_file(
                    tareas[i].ruta_entrada,
                    tareas[i].ruta_salida,
                    md5_guardado
                ) == 0
            ) {
                if (
                    compute_md5(
                        tareas[i].ruta_salida,
                        md5_actual
                    ) == 0
                ) {
                    resultados[i].exitoso = 1;

                    if (
                        memcmp(
                            md5_guardado,
                            md5_actual,
                            MD5_DIGEST_LENGTH
                        ) == 0
                    ) {
                        resultados[i].verificado = 1;
                    }
                }
            }

            _exit(0);
        }

        procesos_activos++;
    }

    while (procesos_activos > 0) {
        wait(NULL);
        procesos_activos--;
    }

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

    printf(
        "TIEMPO_COMP=0.000000\n"
    );

    printf(
        "TIEMPO_DESCOMP=%.6f\n",
        tiempo_fin - tiempo_inicio
    );

    printf(
        "ARCHIVOS=%d\n",
        procesados
    );

    printf(
        "VERIFICADOS=%d\n",
        verificados
    );

    printf(
        "BYTES_ORIG=0\n"
    );

    printf(
        "BYTES_COMP=0\n"
    );

    printf(
        "RADIO=0.000000\n"
    );

    munmap(
        resultados,
        sizeof(Resultado) * cantidad
    );

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
        return comprimir_directorio_paralelo(
            argv[2],
            argv[3]
        );
    }

    if (strcmp(argv[1], "d") == 0) {
        return descomprimir_directorio_paralelo(
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
