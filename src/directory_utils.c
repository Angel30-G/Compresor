#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "directory_utils.h"
#include "huffman.h"
#include "md5_util.h"


/*
 * Verifica si una ruta corresponde a un archivo regular.
 */
static int is_regular_file(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISREG(st.st_mode);
}


/*
 * Crea un directorio si no existe.
 */
static int ensure_directory(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        fprintf(
            stderr,
            "La ruta existe pero no es un directorio: %s\n",
            path
        );

        return -1;
    }

    if (mkdir(path, 0755) != 0) {
        fprintf(
            stderr,
            "No se pudo crear el directorio %s: %s\n",
            path,
            strerror(errno)
        );

        return -1;
    }

    return 0;
}


/*
 * Comprueba si un nombre termina con determinada extension.
 */
static int has_suffix(
    const char *name,
    const char *suffix
) {
    size_t name_length = strlen(name);
    size_t suffix_length = strlen(suffix);

    if (name_length < suffix_length) {
        return 0;
    }

    return strcmp(
        name + name_length - suffix_length,
        suffix
    ) == 0;
}


int compress_directory(
    const char *input_dir,
    const char *output_dir
) {
    DIR *dir = opendir(input_dir);

    if (!dir) {
        fprintf(
            stderr,
            "No se pudo abrir el directorio de entrada: %s\n",
            input_dir
        );

        return -1;
    }

    if (ensure_directory(output_dir) != 0) {
        closedir(dir);
        return -1;
    }

    struct dirent *entry;

    int processed = 0;
    int errors = 0;

    while ((entry = readdir(dir)) != NULL) {

        /*
         * Ignorar . y ..
         * y archivos ocultos como .gitkeep.
         */
        if (entry->d_name[0] == '.') {
            continue;
        }

        char input_path[PATH_MAX];
        char output_path[PATH_MAX];

        int written = snprintf(
            input_path,
            sizeof(input_path),
            "%s/%s",
            input_dir,
            entry->d_name
        );

        if (
            written < 0 ||
            written >= (int)sizeof(input_path)
        ) {
            fprintf(
                stderr,
                "Ruta demasiado larga: %s\n",
                entry->d_name
            );

            errors++;
            continue;
        }

        if (!is_regular_file(input_path)) {
            continue;
        }

        written = snprintf(
            output_path,
            sizeof(output_path),
            "%s/%s.huff",
            output_dir,
            entry->d_name
        );

        if (
            written < 0 ||
            written >= (int)sizeof(output_path)
        ) {
            fprintf(
                stderr,
                "Ruta de salida demasiado larga: %s\n",
                entry->d_name
            );

            errors++;
            continue;
        }

        unsigned char md5_original[MD5_DIGEST_LENGTH];

        if (
            compute_md5(
                input_path,
                md5_original
            ) != 0
        ) {
            fprintf(
                stderr,
                "No se pudo calcular MD5 de: %s\n",
                input_path
            );

            errors++;
            continue;
        }

        long long compressed_size = 0;

        printf(
            "[COMPRESION] %s\n",
            entry->d_name
        );

        if (
            compress_file(
                input_path,
                output_path,
                md5_original,
                &compressed_size
            ) != 0
        ) {
            fprintf(
                stderr,
                "Error comprimiendo: %s\n",
                input_path
            );

            errors++;
            continue;
        }

        processed++;
    }

    closedir(dir);

    printf("\n");
    printf("Archivos comprimidos correctamente: %d\n", processed);
    printf("Errores de compresion: %d\n", errors);

    return errors == 0 ? 0 : -1;
}


int decompress_directory(
    const char *input_dir,
    const char *output_dir
) {
    DIR *dir = opendir(input_dir);

    if (!dir) {
        fprintf(
            stderr,
            "No se pudo abrir el directorio comprimido: %s\n",
            input_dir
        );

        return -1;
    }

    if (ensure_directory(output_dir) != 0) {
        closedir(dir);
        return -1;
    }

    struct dirent *entry;

    int processed = 0;
    int verified = 0;
    int errors = 0;

    while ((entry = readdir(dir)) != NULL) {

        if (entry->d_name[0] == '.') {
            continue;
        }

        /*
         * Solo procesar archivos .huff.
         */
        if (!has_suffix(entry->d_name, ".huff")) {
            continue;
        }

        char input_path[PATH_MAX];
        char output_path[PATH_MAX];

        int written = snprintf(
            input_path,
            sizeof(input_path),
            "%s/%s",
            input_dir,
            entry->d_name
        );

        if (
            written < 0 ||
            written >= (int)sizeof(input_path)
        ) {
            fprintf(
                stderr,
                "Ruta demasiado larga: %s\n",
                entry->d_name
            );

            errors++;
            continue;
        }

        if (!is_regular_file(input_path)) {
            continue;
        }

        /*
         * Eliminar ".huff" para recuperar
         * el nombre original.
         */
        char original_name[NAME_MAX + 1];

        size_t name_length =
            strlen(entry->d_name);

        size_t original_length =
            name_length - strlen(".huff");

        if (original_length > NAME_MAX) {
            fprintf(
                stderr,
                "Nombre demasiado largo: %s\n",
                entry->d_name
            );

            errors++;
            continue;
        }

        memcpy(
            original_name,
            entry->d_name,
            original_length
        );

        original_name[original_length] = '\0';

        written = snprintf(
            output_path,
            sizeof(output_path),
            "%s/%s",
            output_dir,
            original_name
        );

        if (
            written < 0 ||
            written >= (int)sizeof(output_path)
        ) {
            fprintf(
                stderr,
                "Ruta de salida demasiado larga: %s\n",
                original_name
            );

            errors++;
            continue;
        }

        unsigned char md5_stored[MD5_DIGEST_LENGTH];

        printf(
            "[DESCOMPRESION] %s\n",
            entry->d_name
        );

        if (
            decompress_file(
                input_path,
                output_path,
                md5_stored
            ) != 0
        ) {
            fprintf(
                stderr,
                "Error descomprimiendo: %s\n",
                input_path
            );

            errors++;
            continue;
        }

        unsigned char md5_decompressed[
            MD5_DIGEST_LENGTH
        ];

        if (
            compute_md5(
                output_path,
                md5_decompressed
            ) != 0
        ) {
            fprintf(
                stderr,
                "No se pudo calcular MD5 de: %s\n",
                output_path
            );

            errors++;
            continue;
        }

        processed++;

        if (
            memcmp(
                md5_stored,
                md5_decompressed,
                MD5_DIGEST_LENGTH
            ) == 0
        ) {
            verified++;
        } else {
            fprintf(
                stderr,
                "MD5 incorrecto: %s\n",
                output_path
            );

            errors++;
        }
    }

    closedir(dir);

    printf("\n");
    printf("Archivos descomprimidos: %d\n", processed);
    printf("Firmas MD5 verificadas: %d/%d\n", verified, processed);
    printf("Errores de descompresion: %d\n", errors);

    return errors == 0 ? 0 : -1;
}
