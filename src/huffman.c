#include "huffman.h"

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
    // Crear nodos hoja para cada símbolo con frecuencia > 0
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
    if (!root->left && !root->right) {
        buffer[depth] = '\0';
        codes[root->symbol] = (char*)malloc(strlen(buffer) + 1);
        strcpy(codes[root->symbol], buffer);
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
