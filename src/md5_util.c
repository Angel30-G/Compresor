#include <stdio.h>

#include "md5_util.h"

int compute_md5(
    const char *filename,
    unsigned char *md5_result
) {
    FILE *file = fopen(filename, "rb");

    if (!file) {
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    if (!ctx) {
        fclose(file);
        return -1;
    }

    const EVP_MD *md = EVP_md5();

    if (!md) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return -1;
    }

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return -1;
    }

    unsigned char buffer[8192];
    size_t bytes_read;

    while (
        (
            bytes_read = fread(
                buffer,
                1,
                sizeof(buffer),
                file
            )
        ) > 0
    ) {
        if (
            EVP_DigestUpdate(
                ctx,
                buffer,
                bytes_read
            ) != 1
        ) {
            EVP_MD_CTX_free(ctx);
            fclose(file);
            return -1;
        }
    }

    if (ferror(file)) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return -1;
    }

    unsigned int digest_length = 0;

    if (
        EVP_DigestFinal_ex(
            ctx,
            md5_result,
            &digest_length
        ) != 1
    ) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    fclose(file);

    if (digest_length != MD5_DIGEST_LENGTH) {
        return -1;
    }

    return 0;
}

void md5_to_hex(
    const unsigned char *md5,
    char *output
) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(
            &output[i * 2],
            "%02x",
            md5[i]
        );
    }

    output[
        MD5_DIGEST_LENGTH * 2
    ] = '\0';
}
