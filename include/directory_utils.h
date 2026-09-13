#ifndef DIRECTORY_UTILS_H
#define DIRECTORY_UTILS_H

/*
 * Comprime todos los archivos regulares encontrados
 * dentro de input_dir.
 *
 * Cada archivo comprimido se escribe en output_dir
 * agregando la extension .huff.
 *
 * Ejemplo:
 *
 * libro.txt
 * ->
 * libro.txt.huff
 */
int compress_directory(
    const char *input_dir,
    const char *output_dir
);

/*
 * Descomprime todos los archivos .huff encontrados
 * dentro de input_dir.
 *
 * La extension .huff se elimina del nombre de salida.
 *
 * Ejemplo:
 *
 * libro.txt.huff
 * ->
 * libro.txt
 */
int decompress_directory(
    const char *input_dir,
    const char *output_dir
);

#endif
