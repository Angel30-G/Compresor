#include <stdio.h>

#include "md5_util.h"
#include "huffman.h"

int main(void) {
    const char *input =
        "tests/prueba.txt";

    const char *output =
        "data/comprimidos/prueba.huff";

    unsigned char md5[MD5_DIGEST_LENGTH];
    char md5_hex[33];

    if (compute_md5(input, md5) != 0) {
        fprintf(stderr, "No se pudo calcular el MD5\n");
        return 1;
    }

    md5_to_hex(md5, md5_hex);

    printf("Archivo original: %s\n", input);
    printf("MD5 original: %s\n", md5_hex);

    long long compressed_size = 0;

    if (compress_file(
            input,
            output,
            md5,
            &compressed_size
        ) != 0) {

        fprintf(stderr, "Error durante la compresión\n");
        return 1;
    }

    printf("Archivo comprimido: %s\n", output);
    printf(
        "Tamaño comprimido: %lld bytes\n",
        compressed_size
    );

    printf("Compresión finalizada correctamente.\n");

    return 0;
}
