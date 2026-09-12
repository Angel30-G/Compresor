#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nodo del árbol de Huffman
typedef struct Node {
    unsigned char symbol;
    int frequency;
    struct Node *left;
    struct Node *right;
} Node;

// Cola de prioridad (min-heap) para construir el árbol
typedef struct {
    Node **array;
    int size;
    int capacity;
} MinHeap;

// Funciones de la cola de prioridad
MinHeap* create_min_heap(int capacity);
void insert_node(MinHeap *heap, Node *node);
Node* extract_min(MinHeap *heap);
void free_heap(MinHeap *heap);

// Construcción del árbol de Huffman a partir de las frecuencias
Node* build_huffman_tree(unsigned long *freq);

// Generación de códigos (recorrido recursivo)
void generate_codes(Node *root, char **codes, char *buffer, int depth);

// Liberar el árbol
void free_tree(Node *root);

// Función para comprimir un archivo (prototipo)
int compress_file(const char *input_path, const char *output_path, const unsigned char *md5_original, long long *compressed_size);

// Función para descomprimir (prototipo)
int decompress_file(const char *input_path, const char *output_path, unsigned char *md5_expected);

#endif
