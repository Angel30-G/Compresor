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

    // Probar Huffman: construir árbol con frecuencias de ejemplo
    unsigned long freq[256] = {0};
    freq['a'] = 5;
    freq['b'] = 9;
    freq['c'] = 12;
    freq['d'] = 13;
    freq['e'] = 16;
    freq['f'] = 45;

    Node *root = build_huffman_tree(freq);
    char *codes[256] = {0};
    char buffer[256];
    generate_codes(root, codes, buffer, 0);

    for (int i = 0; i < 256; i++) {
        if (codes[i]) {
            printf("'%c': %s\n", i, codes[i]);
            free(codes[i]);
        }
    }
    free_tree(root);
    return 0;
}
