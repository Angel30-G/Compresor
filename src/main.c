#include "directory_utils.h"
#include <stdio.h>
#include <string.h>

#include "md5_util.h"
#include "huffman.h"

typedef struct {
    const char *name;
    const char *original;
    const char *compressed;
    const char *decompressed;
} TestFile;

int run_test(const TestFile *test) {
    unsigned char md5_original[MD5_DIGEST_LENGTH];
    unsigned char md5_stored[MD5_DIGEST_LENGTH];
    unsigned char md5_decompressed[MD5_DIGEST_LENGTH];

    char original_hex[33];
    char stored_hex[33];
    char decompressed_hex[33];

    long long compressed_size = 0;

    printf("\n========================================\n");
    printf("Prueba: %s\n", test->name);
    printf("========================================\n");

    if (compute_md5(test->original, md5_original) != 0) {
        fprintf(stderr, "Error calculando MD5 original.\n");
        return -1;
    }

    md5_to_hex(md5_original, original_hex);

    printf("Archivo original: %s\n", test->original);
    printf("MD5 original: %s\n", original_hex);

    if (
        compress_file(
            test->original,
            test->compressed,
            md5_original,
            &compressed_size
        ) != 0
    ) {
        fprintf(stderr, "Error durante la compresion.\n");
        return -1;
    }

    printf("Compresion: OK\n");
    printf("Tamano comprimido: %lld bytes\n", compressed_size);

    if (
        decompress_file(
            test->compressed,
            test->decompressed,
            md5_stored
        ) != 0
    ) {
        fprintf(stderr, "Error durante la descompresion.\n");
        return -1;
    }

    printf("Descompresion: OK\n");

    if (
        compute_md5(
            test->decompressed,
            md5_decompressed
        ) != 0
    ) {
        fprintf(stderr, "Error calculando MD5 descomprimido.\n");
        return -1;
    }

    md5_to_hex(md5_stored, stored_hex);
    md5_to_hex(md5_decompressed, decompressed_hex);

    printf("MD5 almacenado:     %s\n", stored_hex);
    printf("MD5 descomprimido: %s\n", decompressed_hex);

    if (
        memcmp(
            md5_stored,
            md5_decompressed,
            MD5_DIGEST_LENGTH
        ) != 0
    ) {
        printf("Integridad: ERROR\n");
        return -1;
    }

    printf("Integridad: OK\n");

    return 0;
}

int main(int argc, char *argv[]) {
    /*
     * Modo de compresion de directorio.
     *
     * Ejemplo:
     * ./bin/compresor compress-dir data/originales data/comprimidos
     */
    if (
        argc == 4 &&
        strcmp(argv[1], "compress-dir") == 0
    ) {
        return compress_directory(
            argv[2],
            argv[3]
        ) == 0 ? 0 : 1;
    }

    /*
     * Modo de descompresion de directorio.
     *
     * Ejemplo:
     * ./bin/compresor decompress-dir data/comprimidos data/descomprimidos
     */
    if (
        argc == 4 &&
        strcmp(argv[1], "decompress-dir") == 0
    ) {
        return decompress_directory(
            argv[2],
            argv[3]
        ) == 0 ? 0 : 1;
    }

    /*
     * Si se pasan argumentos incorrectos,
     * mostrar ayuda.
     */
    if (argc != 1) {
        printf(
            "Uso:\n"
            "  %s\n"
            "  %s compress-dir <entrada> <salida>\n"
            "  %s decompress-dir <entrada> <salida>\n",
            argv[0],
            argv[0],
            argv[0]
        );

        return 1;
    }
    TestFile tests[] = {
        {
            "Archivo vacio",
            "tests/vacio.txt",
            "data/comprimidos/vacio.huff",
            "data/descomprimidos/vacio.txt"
        },
        {
            "Un solo simbolo",
            "tests/un_simbolo.txt",
            "data/comprimidos/un_simbolo.huff",
            "data/descomprimidos/un_simbolo.txt"
        },
        {
            "Archivo normal",
            "tests/normal.txt",
            "data/comprimidos/normal.huff",
            "data/descomprimidos/normal.txt"
        },
        {
            "Archivo de prueba original",
            "tests/prueba.txt",
            "data/comprimidos/prueba.huff",
            "data/descomprimidos/prueba.txt"
        },
        {
            "Moby Dick",
            "data/originales/001_2701_Moby_Dick__Or__The_Whale_by_Herman_Melville.txt",
            "data/comprimidos/moby_dick.huff",
            "data/descomprimidos/moby_dick.txt"
        }
    };

    int total_tests =
        sizeof(tests) / sizeof(tests[0]);

    int successful_tests = 0;

    for (int i = 0; i < total_tests; i++) {
        if (run_test(&tests[i]) == 0) {
            successful_tests++;
        }
    }

    printf("\n========================================\n");
    printf("RESUMEN DE PRUEBAS\n");
    printf("========================================\n");
    printf(
        "Pruebas exitosas: %d/%d\n",
        successful_tests,
        total_tests
    );

    if (successful_tests == total_tests) {
        printf("TODAS LAS PRUEBAS PASARON.\n");
        return 0;
    }

    printf("ALGUNAS PRUEBAS FALLARON.\n");

    return 1;
}
