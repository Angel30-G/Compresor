#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"

/*
 * Cuenta la frecuencia de cada byte del archivo.
 */
int count_frequencies(
    const char *input_path,
    unsigned long *freq,
    unsigned long long *total_bytes
) {
    FILE *file = fopen(input_path, "rb");

    if (!file) {
        return -1;
    }

    for (int i = 0; i < 256; i++) {
        freq[i] = 0;
    }

    *total_bytes = 0;

    unsigned char buffer[8192];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            freq[buffer[i]]++;
        }

        *total_bytes += bytes_read;
    }

    if (ferror(file)) {
        fclose(file);
        return -1;
    }

    fclose(file);

    return 0;
}


/*
 * Crea una cola de prioridad tipo min-heap.
 */
MinHeap* create_min_heap(int capacity) {
    MinHeap *heap = malloc(sizeof(MinHeap));

    if (!heap) {
        return NULL;
    }

    heap->array = malloc((size_t)capacity * sizeof(Node*));

    if (!heap->array) {
        free(heap);
        return NULL;
    }

    heap->size = 0;
    heap->capacity = capacity;

    return heap;
}


/*
 * Inserta un nodo en el min-heap.
 */
void insert_node(MinHeap *heap, Node *node) {
    if (!heap || !node) {
        return;
    }

    if (heap->size >= heap->capacity) {
        return;
    }

    int i = heap->size++;

    heap->array[i] = node;

    while (
        i > 0 &&
        heap->array[i]->frequency <
        heap->array[(i - 1) / 2]->frequency
    ) {
        Node *temp = heap->array[i];

        heap->array[i] = heap->array[(i - 1) / 2];
        heap->array[(i - 1) / 2] = temp;

        i = (i - 1) / 2;
    }
}


/*
 * Extrae el nodo con menor frecuencia.
 */
Node* extract_min(MinHeap *heap) {
    if (!heap || heap->size == 0) {
        return NULL;
    }

    Node *min = heap->array[0];

    heap->size--;

    if (heap->size > 0) {
        heap->array[0] = heap->array[heap->size];
    }

    int i = 0;

    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (
            left < heap->size &&
            heap->array[left]->frequency <
            heap->array[smallest]->frequency
        ) {
            smallest = left;
        }

        if (
            right < heap->size &&
            heap->array[right]->frequency <
            heap->array[smallest]->frequency
        ) {
            smallest = right;
        }

        if (smallest == i) {
            break;
        }

        Node *temp = heap->array[i];

        heap->array[i] = heap->array[smallest];
        heap->array[smallest] = temp;

        i = smallest;
    }

    return min;
}


/*
 * Libera el heap.
 */
void free_heap(MinHeap *heap) {
    if (!heap) {
        return;
    }

    free(heap->array);
    free(heap);
}


/*
 * Construye el árbol Huffman usando las frecuencias.
 */
Node* build_huffman_tree(unsigned long *freq) {
    MinHeap *heap = create_min_heap(256);

    if (!heap) {
        return NULL;
    }

    /*
     * Crear un nodo hoja por cada símbolo con frecuencia > 0.
     */
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            Node *node = malloc(sizeof(Node));

            if (!node) {
                free_heap(heap);
                return NULL;
            }

            node->symbol = (unsigned char)i;
            node->frequency = freq[i];
            node->left = NULL;
            node->right = NULL;

            insert_node(heap, node);
        }
    }

    /*
     * Si no hay símbolos, el archivo estaba vacío.
     */
    if (heap->size == 0) {
        free_heap(heap);
        return NULL;
    }

    /*
     * Combinar los dos nodos de menor frecuencia
     * hasta obtener un único árbol.
     */
    while (heap->size > 1) {
        Node *left = extract_min(heap);
        Node *right = extract_min(heap);

        if (!left || !right) {
            free_heap(heap);
            return NULL;
        }

        Node *parent = malloc(sizeof(Node));

        if (!parent) {
            free_tree(left);
            free_tree(right);
            free_heap(heap);
            return NULL;
        }

        parent->symbol = 0;
        parent->frequency =
            left->frequency + right->frequency;

        parent->left = left;
        parent->right = right;

        insert_node(heap, parent);
    }

    Node *root = extract_min(heap);

    free_heap(heap);

    return root;
}


/*
 * Genera los códigos binarios Huffman.
 */
void generate_codes(
    Node *root,
    char **codes,
    char *buffer,
    int depth
) {
    if (!root) {
        return;
    }

    /*
     * Nodo hoja.
     */
    if (!root->left && !root->right) {

        /*
         * Caso especial:
         * si el archivo contiene un único símbolo,
         * Huffman debe asignarle al menos un bit.
         */
        if (depth == 0) {
            buffer[0] = '0';
            depth = 1;
        }

        buffer[depth] = '\0';

        codes[root->symbol] =
            malloc((size_t)depth + 1);

        if (codes[root->symbol]) {
            strcpy(codes[root->symbol], buffer);
        }

        return;
    }

    if (root->left) {
        buffer[depth] = '0';

        generate_codes(
            root->left,
            codes,
            buffer,
            depth + 1
        );
    }

    if (root->right) {
        buffer[depth] = '1';

        generate_codes(
            root->right,
            codes,
            buffer,
            depth + 1
        );
    }
}


/*
 * Libera recursivamente el árbol Huffman.
 */
void free_tree(Node *root) {
    if (!root) {
        return;
    }

    free_tree(root->left);
    free_tree(root->right);

    free(root);
}


/*
 * Comprime un archivo usando Huffman.
 *
 * Formato generado:
 *
 * 4 bytes      -> magic "HUF1"
 * 8 bytes      -> tamaño original
 * 16 bytes     -> MD5 original
 * 2048 bytes   -> tabla de 256 frecuencias uint64_t
 * resto        -> datos Huffman
 */
int compress_file(
    const char *input_path,
    const char *output_path,
    const unsigned char *md5_original,
    long long *compressed_size
) {
    unsigned long freq[256];
    unsigned long long total_bytes = 0;

    if (
        count_frequencies(
            input_path,
            freq,
            &total_bytes
        ) != 0
    ) {
        fprintf(
            stderr,
            "Error al contar frecuencias de %s\n",
            input_path
        );

        return -1;
    }

    Node *root = NULL;

    char *codes[256] = {0};
    char code_buffer[256];

    /*
     * Si el archivo no está vacío,
     * construimos árbol y códigos.
     */
    if (total_bytes > 0) {
        root = build_huffman_tree(freq);

        if (!root) {
            fprintf(
                stderr,
                "Error al construir el árbol Huffman\n"
            );

            return -1;
        }

        generate_codes(
            root,
            codes,
            code_buffer,
            0
        );
    }

    FILE *input = fopen(input_path, "rb");

    if (!input) {
        free_tree(root);

        return -1;
    }

    FILE *output = fopen(output_path, "wb");

    if (!output) {
        fclose(input);
        free_tree(root);

        return -1;
    }

    /*
     * Magic number del archivo.
     */
    const unsigned char magic[4] = {
        'H',
        'U',
        'F',
        '1'
    };

    if (fwrite(magic, 1, 4, output) != 4) {
        fclose(input);
        fclose(output);
        free_tree(root);

        return -1;
    }

    /*
     * Guardar tamaño original.
     */
    uint64_t original_size =
        (uint64_t)total_bytes;

    if (
        fwrite(
            &original_size,
            sizeof(original_size),
            1,
            output
        ) != 1
    ) {
        fclose(input);
        fclose(output);
        free_tree(root);

        return -1;
    }

    /*
     * Guardar MD5 del archivo original.
     */
    if (
        fwrite(
            md5_original,
            1,
            16,
            output
        ) != 16
    ) {
        fclose(input);
        fclose(output);
        free_tree(root);

        return -1;
    }

    /*
     * Guardar tabla de frecuencias.
     */
    for (int i = 0; i < 256; i++) {
        uint64_t value =
            (uint64_t)freq[i];

        if (
            fwrite(
                &value,
                sizeof(value),
                1,
                output
            ) != 1
        ) {
            fclose(input);
            fclose(output);
            free_tree(root);

            for (int j = 0; j < 256; j++) {
                free(codes[j]);
            }

            return -1;
        }
    }

    /*
     * Escribir los datos comprimidos bit a bit.
     */
    unsigned char output_byte = 0;
    int bit_count = 0;

    unsigned char input_buffer[8192];
    size_t bytes_read;

    while (
        (
            bytes_read = fread(
                input_buffer,
                1,
                sizeof(input_buffer),
                input
            )
        ) > 0
    ) {
        for (
            size_t i = 0;
            i < bytes_read;
            i++
        ) {
            const char *code =
                codes[input_buffer[i]];

            if (!code) {
                fclose(input);
                fclose(output);

                free_tree(root);

                for (
                    int j = 0;
                    j < 256;
                    j++
                ) {
                    free(codes[j]);
                }

                return -1;
            }

            for (
                size_t j = 0;
                code[j] != '\0';
                j++
            ) {
                /*
                 * Desplazamos el byte a la izquierda
                 * y agregamos el siguiente bit.
                 */
                output_byte <<= 1;

                if (code[j] == '1') {
                    output_byte |= 1;
                }

                bit_count++;

                /*
                 * Cuando completamos 8 bits,
                 * escribimos un byte.
                 */
                if (bit_count == 8) {
                    if (
                        fwrite(
                            &output_byte,
                            1,
                            1,
                            output
                        ) != 1
                    ) {
                        fclose(input);
                        fclose(output);

                        free_tree(root);

                        for (
                            int k = 0;
                            k < 256;
                            k++
                        ) {
                            free(codes[k]);
                        }

                        return -1;
                    }

                    output_byte = 0;
                    bit_count = 0;
                }
            }
        }
    }

    if (ferror(input)) {
        fclose(input);
        fclose(output);

        free_tree(root);

        for (int i = 0; i < 256; i++) {
            free(codes[i]);
        }

        return -1;
    }

    /*
     * Si quedan bits pendientes,
     * completar el último byte con ceros.
     */
    if (bit_count > 0) {
        output_byte <<= (8 - bit_count);

        if (
            fwrite(
                &output_byte,
                1,
                1,
                output
            ) != 1
        ) {
            fclose(input);
            fclose(output);

            free_tree(root);

            for (int i = 0; i < 256; i++) {
                free(codes[i]);
            }

            return -1;
        }
    }

    fclose(input);

    /*
     * Obtener tamaño final del archivo comprimido.
     */
    long position = ftell(output);

    if (
        position >= 0 &&
        compressed_size != NULL
    ) {
        *compressed_size =
            (long long)position;
    }

    fclose(output);

    /*
     * Liberar códigos.
     */
    for (int i = 0; i < 256; i++) {
        free(codes[i]);
    }

    free_tree(root);

    return 0;
}


/*
 * Descomprime un archivo generado por compress_file().
 */
int decompress_file(
    const char *input_path,
    const char *output_path,
    unsigned char *md5_expected
) {
    FILE *input =
        fopen(input_path, "rb");

    if (!input) {
        fprintf(
            stderr,
            "No se pudo abrir el archivo comprimido: %s\n",
            input_path
        );

        return -1;
    }

    /*
     * Leer y validar magic number.
     */
    unsigned char magic[4];

    if (
        fread(
            magic,
            1,
            4,
            input
        ) != 4
    ) {
        fprintf(
            stderr,
            "No se pudo leer la cabecera del archivo.\n"
        );

        fclose(input);

        return -1;
    }

    if (
        memcmp(
            magic,
            "HUF1",
            4
        ) != 0
    ) {
        fprintf(
            stderr,
            "El archivo no tiene un formato Huffman válido.\n"
        );

        fclose(input);

        return -1;
    }

    /*
     * Leer tamaño original.
     */
    uint64_t original_size;

    if (
        fread(
            &original_size,
            sizeof(original_size),
            1,
            input
        ) != 1
    ) {
        fprintf(
            stderr,
            "No se pudo leer el tamaño original.\n"
        );

        fclose(input);

        return -1;
    }

    /*
     * Leer MD5 guardado.
     */
    if (
        fread(
            md5_expected,
            1,
            16,
            input
        ) != 16
    ) {
        fprintf(
            stderr,
            "No se pudo leer el MD5 almacenado.\n"
        );

        fclose(input);

        return -1;
    }

    /*
     * Leer tabla de frecuencias.
     */
    unsigned long freq[256] = {0};

    uint64_t frequency_sum = 0;

    for (int i = 0; i < 256; i++) {
        uint64_t value;

        if (
            fread(
                &value,
                sizeof(value),
                1,
                input
            ) != 1
        ) {
            fprintf(
                stderr,
                "Error al leer la tabla de frecuencias.\n"
            );

            fclose(input);

            return -1;
        }

        freq[i] =
            (unsigned long)value;

        frequency_sum += value;
    }

    /*
     * Verificar consistencia.
     */
    if (frequency_sum != original_size) {
        fprintf(
            stderr,
            "Archivo Huffman corrupto: frecuencias inválidas.\n"
        );

        fclose(input);

        return -1;
    }

    FILE *output =
        fopen(output_path, "wb");

    if (!output) {
        fprintf(
            stderr,
            "No se pudo crear el archivo descomprimido: %s\n",
            output_path
        );

        fclose(input);

        return -1;
    }

    /*
     * Caso especial:
     * archivo original vacío.
     */
    if (original_size == 0) {
        fclose(input);
        fclose(output);

        return 0;
    }

    /*
     * Reconstruir árbol Huffman.
     */
    Node *root =
        build_huffman_tree(freq);

    if (!root) {
        fprintf(
            stderr,
            "No se pudo reconstruir el árbol Huffman.\n"
        );

        fclose(input);
        fclose(output);

        return -1;
    }

    /*
     * Caso especial:
     * archivo formado por un único símbolo.
     */
    if (
        !root->left &&
        !root->right
    ) {
        for (
            uint64_t i = 0;
            i < original_size;
            i++
        ) {
            if (
                fwrite(
                    &root->symbol,
                    1,
                    1,
                    output
                ) != 1
            ) {
                fprintf(
                    stderr,
                    "Error al escribir el archivo descomprimido.\n"
                );

                free_tree(root);

                fclose(input);
                fclose(output);

                return -1;
            }
        }

        free_tree(root);

        fclose(input);
        fclose(output);

        return 0;
    }

    /*
     * Recorrer los datos comprimidos bit a bit.
     */
    Node *current = root;

    uint64_t bytes_written = 0;

    unsigned char byte;

    while (
        bytes_written < original_size &&
        fread(
            &byte,
            1,
            1,
            input
        ) == 1
    ) {
        /*
         * Leer desde el bit más significativo
         * hasta el menos significativo.
         */
        for (
            int bit_position = 7;
            bit_position >= 0;
            bit_position--
        ) {
            int bit =
                (byte >> bit_position) & 1;

            if (bit == 0) {
                current = current->left;
            } else {
                current = current->right;
            }

            /*
             * Si el recorrido llega a NULL,
             * los datos están dañados.
             */
            if (!current) {
                fprintf(
                    stderr,
                    "Archivo Huffman corrupto: ruta inválida en el árbol.\n"
                );

                free_tree(root);

                fclose(input);
                fclose(output);

                return -1;
            }

            /*
             * Si llegamos a una hoja,
             * recuperamos un símbolo.
             */
            if (
                !current->left &&
                !current->right
            ) {
                if (
                    fwrite(
                        &current->symbol,
                        1,
                        1,
                        output
                    ) != 1
                ) {
                    fprintf(
                        stderr,
                        "Error al escribir el archivo descomprimido.\n"
                    );

                    free_tree(root);

                    fclose(input);
                    fclose(output);

                    return -1;
                }

                bytes_written++;

                current = root;

                /*
                 * Ya recuperamos exactamente
                 * el tamaño original.
                 */
                if (
                    bytes_written ==
                    original_size
                ) {
                    break;
                }
            }
        }
    }

    free_tree(root);

    fclose(input);
    fclose(output);

    /*
     * Si no recuperamos todos los bytes,
     * el archivo comprimido estaba incompleto.
     */
    if (
        bytes_written !=
        original_size
    ) {
        fprintf(
            stderr,
            "El archivo comprimido terminó antes de recuperar todos los datos.\n"
        );

        return -1;
    }

    return 0;
}
