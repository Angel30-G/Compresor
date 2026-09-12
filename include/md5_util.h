#ifndef MD5_UTIL_H
#define MD5_UTIL_H

#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>

#define MD5_DIGEST_LENGTH 16

// Calcula el MD5 de un archivo y lo guarda en md5_result (debe tener 16 bytes)
int compute_md5(const char *filename, unsigned char *md5_result);

// Convierte un hash MD5 binario a una cadena hexadecimal (para imprimir)
void md5_to_hex(const unsigned char *md5, char *hex_output);

#endif
