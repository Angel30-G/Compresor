#include <stdio.h>
#include <string.h>

#include "md5_util.h"
#include "huffman.h"

int main(void) {
    const char *original_path =
	 "data/originales/001_2701_Moby_Dick__Or__The_Whale_by_Herman_Melville.txt";

    const char *compressed_path =
    	"data/comprimidos/moby_dick.huff";

    const char *decompressed_path =
    	"data/descomprimidos/moby_dick_recuperado.txt";

    unsigned char md5_original[MD5_DIGEST_LENGTH];
    unsigned char md5_stored[MD5_DIGEST_LENGTH];
    unsigned char md5_decompressed[MD5_DIGEST_LENGTH];

    char original_hex[33];
    char stored_hex[33];
    char decompressed_hex[33];

    /*
     * 1. Calcular MD5 del archivo original.
     */
    if (compute_md5(original_path, md5_original) != 0) {
        fprintf(stderr, "No se pudo calcular el MD5 del archivo original.\n");
        return 1;
    }

    md5_to_hex(md5_original, original_hex);

    printf("Archivo original: %s\n", original_path);
    printf("MD5 original: %s\n\n", original_hex);

    /*
     * 2. Comprimir.
     */
    long long compressed_size = 0;

    if (compress_file(
            original_path,
            compressed_path,
            md5_original,
            &compressed_size
        ) != 0) {

        fprintf(stderr, "Error durante la compresión.\n");
        return 1;
    }

    printf("Compresión finalizada correctamente.\n");
    printf("Archivo comprimido: %s\n", compressed_path);
    printf("Tamaño comprimido: %lld bytes\n\n", compressed_size);

    /*
     * 3. Descomprimir.
     */
    if (decompress_file(
            compressed_path,
            decompressed_path,
            md5_stored
        ) != 0) {

        fprintf(stderr, "Error durante la descompresión.\n");
        return 1;
    }

    printf("Descompresión finalizada correctamente.\n");
    printf("Archivo recuperado: %s\n\n", decompressed_path);

    /*
     * 4. Calcular MD5 del archivo recuperado.
     */
    if (compute_md5(
            decompressed_path,
            md5_decompressed
        ) != 0) {

        fprintf(stderr, "No se pudo calcular el MD5 del archivo recuperado.\n");
        return 1;
    }

    md5_to_hex(md5_stored, stored_hex);
    md5_to_hex(md5_decompressed, decompressed_hex);

    printf("MD5 almacenado:     %s\n", stored_hex);
    printf("MD5 descomprimido: %s\n", decompressed_hex);

    /*
     * 5. Comparar ambas firmas.
     */
    if (memcmp(
            md5_stored,
            md5_decompressed,
            MD5_DIGEST_LENGTH
        ) == 0) {

        printf("\nIntegridad: OK\n");
        printf("El archivo descomprimido es idéntico al original.\n");

    } else {

        printf("\nIntegridad: ERROR\n");
        printf("Los MD5 no coinciden.\n");

        return 1;
    }

    return 0;
}
