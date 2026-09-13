#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nodo del arbol de Huffman
typedef struct Node {
    unsigned char symbol;
    unsigned long frequency;
    struct Node *left;
    struct Node *right;
} Node;

// Cola de prioridad (min-heap) para construir el arbol
typedef struct {
    Node **array;
    int size;
    int capacity;
} MinHeap;

// Funciones de la cola de prioridad
MinHeap* create_min_heap(int capacity);

void insert_node(
    MinHeap *heap,
    Node *node
);

Node* extract_min(
    MinHeap *heap
);

void free_heap(
    MinHeap *heap
);

// Contar frecuencias de los bytes de un archivo
int count_frequencies(
    const char *input_path,
    unsigned long *freq,
    unsigned long long *total_bytes
);

// Construccion del arbol de Huffman
Node* build_huffman_tree(
    unsigned long *freq
);

// Generacion de codigos Huffman
void generate_codes(
    Node *root,
    char **codes,
    char *buffer,
    int depth
);

// Liberar el arbol Huffman
void free_tree(
    Node *root
);

// Comprimir un archivo
int compress_file(
    const char *input_path,
    const char *output_path,
    const unsigned char *md5_original,
    long long *compressed_size
);

// Descomprimir un archivo
int decompress_file(
    const char *input_path,
    const char *output_path,
    unsigned char *md5_expected
);

#endif
