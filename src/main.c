#include <stdio.h>
#include "md5_util.h"
#include "huffman.h"

int main() {
    // Probar MD5
    unsigned char md5[MD5_DIGEST_LENGTH];
    char hex[33];
    if (compute_md5("src/main.c", md5) == 0) {
        md5_to_hex(md5, hex);
        printf("MD5 de src/main.c: %s\n", hex);
    } else {
        printf("No se pudo calcular MD5\n");
    }

    // Probar Huffman: construir arbol con frecuencias de ejemplo
    unsigned long freq[256];
    unsigned long long total_bytes = 0;

    if (count_frequencies("tests/prueba.txt", freq, &total_bytes) != 0) {
    	printf("Error al leer el archivo de prueba\n");
    	return 1;
   }

    printf("\nTotal de bytes: %llu\n", total_bytes);
    printf("Frecuencias encontradas:\n");

    for (int i = 0; i < 256; i++) {
    	if (freq[i] > 0) {
        	if (i >= 32 && i <= 126) {
            		printf("'%c': %lu\n", i, freq[i]);
        	} else {
            		printf("Byte %d: %lu\n", i, freq[i]);
        	}
    	}
   }

    Node *root = build_huffman_tree(freq);
    char *codes[256] = {0};
    char buffer[256];
    generate_codes(root, codes, buffer, 0);

   for (int i = 0; i < 256; i++) {
    if (codes[i]) {
        if (i >= 32 && i <= 126) {
            printf("'%c': %s\n", i, codes[i]);
        } else {
            printf("Byte %d: %s\n", i, codes[i]);
        }

        free(codes[i]);
    }
}

}
