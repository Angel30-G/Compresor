#include "md5_util.h"

int compute_md5(const char *filename, unsigned char *md5_result) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    MD5_CTX ctx;
    MD5_Init(&ctx);

    unsigned char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        MD5_Update(&ctx, buffer, bytes);
    }

    MD5_Final(md5_result, &ctx);
    fclose(f);
    return 0;
}

void md5_to_hex(const unsigned char *md5, char *hex_output) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hex_output + i*2, "%02x", md5[i]);
    }
    hex_output[MD5_DIGEST_LENGTH*2] = '\0';
}
