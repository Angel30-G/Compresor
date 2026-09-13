#ifndef MD5_UTIL_H
#define MD5_UTIL_H

#include <openssl/evp.h>

#ifndef MD5_DIGEST_LENGTH
#define MD5_DIGEST_LENGTH 16
#endif

int compute_md5(
    const char *filename,
    unsigned char *md5_result
);

void md5_to_hex(
    const unsigned char *md5,
    char *output
);

#endif
