#include <stdint.h>
#include "huffman.h"

int count_frequencies(const char *input_path, unsigned long *freq, unsigned long long *total_bytes) {
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

MinHeap* create_min_heap(int capacity) {
    MinHeap *heap = (MinHeap*)malloc(sizeof(MinHeap));
    heap->array = (Node**)malloc(capacity * sizeof(Node*));
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void insert_node(MinHeap *heap, Node *node) {
    if (heap->size >= heap->capacity) return;
    int i = heap->size++;
    heap->array[i] = node;
    // Subir para mantener propiedad de min-heap
    while (i > 0 && heap->array[i]->frequency < heap->array[(i-1)/2]->frequency) {
        Node *temp = heap->array[i];
        heap->array[i] = heap->array[(i-1)/2];
        heap->array[(i-1)/2] = temp;
        i = (i-1)/2;
    }
}

Node* extract_min(MinHeap *heap) {
    if (heap->size == 0) return NULL;
    Node *min = heap->array[0];
    heap->array[0] = heap->array[--heap->size];
    // Reordenar hacia abajo
    int i = 0;
    while (1) {
        int left = 2*i + 1;
        int right = 2*i + 2;
        int smallest = i;
        if (left < heap->size && heap->array[left]->frequency < heap->array[smallest]->frequency)
            smallest = left;
        if (right < heap->size && heap->array[right]->frequency < heap->array[smallest]->frequency)
            smallest = right;
        if (smallest == i) break;
        Node *temp = heap->array[i];
        heap->array[i] = heap->array[smallest];
        heap->array[smallest] = temp;
        i = smallest;
    }
    return min;
}

void free_heap(MinHeap *heap) {
    free(heap->array);
    free(heap);
}

Node* build_huffman_tree(unsigned long *freq) {
    MinHeap *heap = create_min_heap(256);
    // Crear nodos hoja para cada s�mbolo con frecuencia > 0
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            Node *node = (Node*)malloc(sizeof(Node));
            node->symbol = (unsigned char)i;
            node->frequency = freq[i];
            node->left = node->right = NULL;
            insert_node(heap, node);
        }
    }

    while (heap->size > 1) {
        Node *left = extract_min(heap);
        Node *right = extract_min(heap);
        Node *parent = (Node*)malloc(sizeof(Node));
        parent->symbol = 0; // no usado para nodos internos
        parent->frequency = left->frequency + right->frequency;
        parent->left = left;
        parent->right = right;
        insert_node(heap, parent);
    }

    Node *root = extract_min(heap);
    free_heap(heap);
    return root;
}

void generate_codes(Node *root, char **codes, char *buffer, int depth) {
    if (!root) {
        return;
    }

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

        codes[root->symbol] = malloc((size_t)depth + 1);

        if (codes[root->symbol] != NULL) {
            strcpy(codes[root->symbol], buffer);
        }

        return;
    }

    if (root->left) {
        buffer[depth] = '0';
        generate_codes(root->left, codes, buffer, depth + 1);
    }

    if (root->right) {
        buffer[depth] = '1';
        generate_codes(root->right, codes, buffer, depth + 1);
    }
}

void free_tree(Node *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int compress_file(
    const char *input_path,
    const char *output_path,
    const unsigned char *md5_original,
    long long *compressed_size
) {
    unsigned long freq[256];
    unsigned long long total_bytes = 0;

    if (count_frequencies(input_path, freq, &total_bytes) != 0) {
        fprintf(stderr, "Error al contar frecuencias de %s\n", input_path);
        return -1;
    }

    Node *root = NULL;
    char *codes[256] = {0};
    char code_buffer[256];

    if (total_bytes > 0) {
        root = build_huffman_tree(freq);

        if (!root) {
            fprintf(stderr, "Error al construir el árbol Huffman\n");
            return -1;
        }

        generate_codes(root, codes, code_buffer, 0);
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

    /* Firma del formato */
    const unsigned char magic[4] = {'H', 'U', 'F', '1'};

    if (fwrite(magic, 1, 4, output) != 4) {
        fclose(input);
        fclose(output);
        free_tree(root);
        return -1;
    }

    /* Tamaño original */
    uint64_t original_size = (uint64_t)total_bytes;

    if (fwrite(&original_size, sizeof(original_size), 1, output) != 1) {
        fclose(input);
        fclose(output);
        free_tree(root);
        return -1;
    }

    /* MD5 original */
    if (fwrite(md5_original, 1, 16, output) != 16) {
        fclose(input);
        fclose(output);
        free_tree(root);
        return -1;
    }

    /* Tabla de frecuencias */
    for (int i = 0; i < 256; i++) {
        uint64_t value = (uint64_t)freq[i];

        if (fwrite(&value, sizeof(value), 1, output) != 1) {
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
     * Datos comprimidos.
     * Se van acumulando bits hasta formar un byte completo.
     */
    unsigned char output_byte = 0;
    int bit_count = 0;

    unsigned char input_buffer[8192];
    size_t bytes_read;

    while ((bytes_read = fread(
        input_buffer,
        1,
        sizeof(input_buffer),
        input
    )) > 0) {

        for (size_t i = 0; i < bytes_read; i++) {
            const char *code = codes[input_buffer[i]];

            if (!code) {
                fclose(input);
                fclose(output);
                free_tree(root);

                for (int j = 0; j < 256; j++) {
                    free(codes[j]);
                }

                return -1;
            }

            for (size_t j = 0; code[j] != '\0'; j++) {
                output_byte <<= 1;

                if (code[j] == '1') {
                    output_byte |= 1;
                }

                bit_count++;

                if (bit_count == 8) {
                    if (fwrite(&output_byte, 1, 1, output) != 1) {
                        fclose(input);
                        fclose(output);
                        free_tree(root);

                        for (int k = 0; k < 256; k++) {
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
     * Si quedaron bits pendientes, rellenamos con ceros
     * la parte derecha del último byte.
     */
    if (bit_count > 0) {
        output_byte <<= (8 - bit_count);

        if (fwrite(&output_byte, 1, 1, output) != 1) {
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
     * Guardamos el tamaño final antes de cerrar.
     */
    long position = ftell(output);

    if (position >= 0 && compressed_size != NULL) {
        *compressed_size = (long long)position;
    }

    fclose(output);

    for (int i = 0; i < 256; i++) {
        free(codes[i]);
    }

    free_tree(root);

    return 0;
}
