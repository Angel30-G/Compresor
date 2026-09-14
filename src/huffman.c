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

/* ============ Serialización del árbol ============ */
static void serialize_tree_rec(Node *root, unsigned char *buf, int *pos) {
    if (!root) return;
    if (!root->left && !root->right) {
        buf[(*pos)++] = 1;
        buf[(*pos)++] = root->symbol;
    } else {
        buf[(*pos)++] = 0;
        serialize_tree_rec(root->left, buf, pos);
        serialize_tree_rec(root->right, buf, pos);
    }
}

static Node* deserialize_tree_rec(unsigned char *buf, int *pos) {
    unsigned char type = buf[(*pos)++];
    Node *node = (Node*)malloc(sizeof(Node));
    node->frequency = 0;
    node->left = node->right = NULL;
    if (type == 1) {
        node->symbol = buf[(*pos)++];
    } else {
        node->symbol = 0;
        node->left = deserialize_tree_rec(buf, pos);
        node->right = deserialize_tree_rec(buf, pos);
    }
    return node;
}

/* ============ Comprimir un archivo ============ */
int compress_file(const char *input_path, const char *output_path,
                  const unsigned char *md5_original, long long *compressed_size) {
    FILE *fin = fopen(input_path, "rb");
    if (!fin) return -1;

    fseek(fin, 0, SEEK_END);
    long long original_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    FILE *fout = fopen(output_path, "wb");
    if (!fout) { fclose(fin); return -1; }

    fwrite(md5_original, 1, 16, fout);
    fwrite(&original_size, sizeof(long long), 1, fout);

    if (original_size == 0) {
        int tree_size = 0;
        long long total_bits = 0;
        fwrite(&tree_size, sizeof(int), 1, fout);
        fwrite(&total_bits, sizeof(long long), 1, fout);
        fclose(fin);
        fseek(fout, 0, SEEK_END);
        if (compressed_size) *compressed_size = ftell(fout);
        fclose(fout);
        return 0;
    }

    unsigned char *data = (unsigned char*)malloc(original_size);
    if (!data) { fclose(fin); fclose(fout); return -1; }
    if (fread(data, 1, original_size, fin) != (size_t)original_size) {
        free(data); fclose(fin); fclose(fout); return -1;
    }
    fclose(fin);

    unsigned long freq[256] = {0};
    for (long long i = 0; i < original_size; i++) freq[data[i]]++;

    Node *root = build_huffman_tree(freq);
    char *codes[256] = {0};
    char buffer[256];
    generate_codes(root, codes, buffer, 0);

    unsigned char tree_buf[2048];
    int tree_pos = 0;
    serialize_tree_rec(root, tree_buf, &tree_pos);
    int tree_size = tree_pos;
    fwrite(&tree_size, sizeof(int), 1, fout);
    fwrite(tree_buf, 1, tree_size, fout);

    long long total_bits = 0;
    for (long long i = 0; i < original_size; i++) {
        if (codes[data[i]]) total_bits += strlen(codes[data[i]]);
    }
    fwrite(&total_bits, sizeof(long long), 1, fout);

    unsigned char byte = 0;
    int bit_count = 0;
    for (long long i = 0; i < original_size; i++) {
        char *code = codes[data[i]];
        if (!code) continue;
        for (int j = 0; code[j]; j++) {
            if (code[j] == '1') byte |= (1 << (7 - bit_count));
            bit_count++;
            if (bit_count == 8) {
                fwrite(&byte, 1, 1, fout);
                byte = 0;
                bit_count = 0;
            }
        }
    }
    if (bit_count > 0) fwrite(&byte, 1, 1, fout);

    for (int i = 0; i < 256; i++) if (codes[i]) free(codes[i]);
    free_tree(root);
    free(data);

    fseek(fout, 0, SEEK_END);
    if (compressed_size) *compressed_size = ftell(fout);
    fclose(fout);
    return 0;
}

/* ============ Descomprimir un archivo ============ */
int decompress_file(const char *input_path, const char *output_path,
                    unsigned char *md5_expected) {
    FILE *fin = fopen(input_path, "rb");
    if (!fin) return -1;

    if (fread(md5_expected, 1, 16, fin) != 16) { fclose(fin); return -1; }

    long long original_size;
    if (fread(&original_size, sizeof(long long), 1, fin) != 1) { fclose(fin); return -1; }

    FILE *fout = fopen(output_path, "wb");
    if (!fout) { fclose(fin); return -1; }

    if (original_size == 0) {
        fclose(fin); fclose(fout); return 0;
    }

    int tree_size;
    if (fread(&tree_size, sizeof(int), 1, fin) != 1) { fclose(fin); fclose(fout); return -1; }
    unsigned char *tree_buf = (unsigned char*)malloc(tree_size);
    if (!tree_buf) { fclose(fin); fclose(fout); return -1; }
    if (fread(tree_buf, 1, tree_size, fin) != (size_t)tree_size) {
        free(tree_buf); fclose(fin); fclose(fout); return -1;
    }
    int tree_pos = 0;
    Node *root = deserialize_tree_rec(tree_buf, &tree_pos);
    free(tree_buf);

    long long total_bits;
    if (fread(&total_bits, sizeof(long long), 1, fin) != 1) {
        free_tree(root); fclose(fin); fclose(fout); return -1;
    }

    if (!root->left && !root->right) {
        unsigned char sym = root->symbol;
        for (long long i = 0; i < original_size; i++) fwrite(&sym, 1, 1, fout);
        free_tree(root); fclose(fin); fclose(fout); return 0;
    }

    Node *current = root;
    long long bits_read = 0;
    unsigned char byte = 0;
    int bit_count = 0;

    while (bits_read < total_bits) {
        if (bit_count == 0) {
            if (fread(&byte, 1, 1, fin) != 1) break;
            bit_count = 8;
        }
        int bit = (byte >> 7) & 1;
        byte = (byte << 1) & 0xFF;
        bit_count--;
        bits_read++;

        current = (bit == 0) ? current->left : current->right;
        if (!current) break;

        if (!current->left && !current->right) {
            fwrite(&current->symbol, 1, 1, fout);
            current = root;
        }
    }

    free_tree(root);
    fclose(fin);
    fclose(fout);
    return 0;
}
