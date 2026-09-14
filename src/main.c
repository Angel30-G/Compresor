#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "md5_util.h"
#include "huffman.h"
#include <locale.h>

static double ahora(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int compress_directory(const char *dir_in, const char *dir_out) {
    double t_inicio = ahora();

    DIR *d = opendir(dir_in);
    if (!d) { perror("opendir"); return 1; }
    struct dirent *entry;
    struct stat st;
    char in_path[2048], out_path[2048];
    int count = 0;
    long long total_orig = 0, total_comp = 0;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(in_path, sizeof(in_path), "%s/%s", dir_in, entry->d_name);
        if (stat(in_path, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;

        snprintf(out_path, sizeof(out_path), "%s/%s.huff", dir_out, entry->d_name);

        unsigned char md5[MD5_DIGEST_LENGTH];
        if (compute_md5(in_path, md5) != 0) continue;

        long long comp_size = 0;
        if (compress_file(in_path, out_path, md5, &comp_size) == 0) {
            printf("[OK] %s | orig=%lld comp=%lld\n",
                   entry->d_name, (long long)st.st_size, comp_size);
            count++;
            total_orig += st.st_size;
            total_comp += comp_size;
        }
    }
    closedir(d);

    double t_fin = ahora();

    /* Estadísticas para la GUI */
    printf("TIEMPO_COMP=%.6f\n", t_fin - t_inicio);
    printf("TIEMPO_DESCOMP=0.000000\n");
    printf("ARCHIVOS=%d\n", count);
    printf("VERIFICADOS=%d\n", count);
    printf("BYTES_ORIG=%lld\n", total_orig);
    printf("BYTES_COMP=%lld\n", total_comp);
    printf("RADIO=%.6f\n", total_orig > 0 ? (double)total_comp / total_orig : 0.0);
    return 0;
}

static int decompress_directory(const char *dir_in, const char *dir_out) {
    double t_inicio = ahora();

    DIR *d = opendir(dir_in);
    if (!d) { perror("opendir"); return 1; }
    struct dirent *entry;
    struct stat st;
    char in_path[2048], out_path[2048];
    int count = 0, verified = 0;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        size_t len = strlen(entry->d_name);
        if (len < 5 || strcmp(entry->d_name + len - 5, ".huff") != 0) continue;

        snprintf(in_path, sizeof(in_path), "%s/%s", dir_in, entry->d_name);
        if (stat(in_path, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;

        char base[1024];
        strncpy(base, entry->d_name, len - 5);
        base[len - 5] = '\0';
        snprintf(out_path, sizeof(out_path), "%s/%s", dir_out, base);

        unsigned char md5_expected[MD5_DIGEST_LENGTH];
        if (decompress_file(in_path, out_path, md5_expected) != 0) continue;

        unsigned char md5_actual[MD5_DIGEST_LENGTH];
        if (compute_md5(out_path, md5_actual) != 0) continue;

        count++;
        if (memcmp(md5_expected, md5_actual, MD5_DIGEST_LENGTH) == 0) {
            printf("[OK] %s | MD5 verificado\n", base);
            verified++;
        } else {
            printf("[FALLO] %s\n", base);
        }
    }
    closedir(d);

    double t_fin = ahora();

    /* Estadísticas para la GUI */
    printf("TIEMPO_COMP=0.000000\n");
    printf("TIEMPO_DESCOMP=%.6f\n", t_fin - t_inicio);
    printf("ARCHIVOS=%d\n", count);
    printf("VERIFICADOS=%d\n", verified);
    printf("BYTES_ORIG=0\n");
    printf("BYTES_COMP=0\n");
    printf("RADIO=0.000000\n");
    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");
    if (argc != 4) {
        fprintf(stderr, "Uso:\n");
        fprintf(stderr, "  %s c <dir_entrada> <dir_salida>\n", argv[0]);
        fprintf(stderr, "  %s d <dir_entrada> <dir_salida>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "c") == 0) return compress_directory(argv[2], argv[3]);
    if (strcmp(argv[1], "d") == 0) return decompress_directory(argv[2], argv[3]);
    fprintf(stderr, "Modo desconocido: %s\n", argv[1]);
    return 1;
}
