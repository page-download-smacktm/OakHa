#include <bearssl_hash.h>
#include <bearssl_ssl.h>
#include "acorn/tls.h"
#include "acorn/entropy.h"

static int same_bytes(const unsigned char *left, const unsigned char *right,
    unsigned int length)
{
    for (unsigned int index = 0; index < length; ++index)
        if (left[index] != right[index]) return 0;
    return 1;
}

int tls_crypto_self_test(void)
{
    static const unsigned char input[] = "abc";
    static const unsigned char expected[32] = {
        0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
        0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
        0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
        0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
    };
    unsigned char digest[32];
    br_sha256_context context;
    br_sha256_init(&context);
    br_sha256_update(&context, input, sizeof(input) - 1);
    br_sha256_out(&context, digest);
    return same_bytes(digest, expected, sizeof(expected));
}

int tls_client_self_test(void)
{
    static br_ssl_client_context client;
    static br_x509_minimal_context x509;
    static unsigned char io_buffer[BR_SSL_BUFSIZE_MONO];
    static unsigned char entropy[32];
    if (!entropy_fill(entropy, sizeof(entropy))) return 0;
    br_ssl_client_init_full(&client, &x509, (const br_x509_trust_anchor *)0, 0);
    br_ssl_engine_set_buffer(&client.eng, io_buffer, sizeof(io_buffer), 0);
    br_ssl_engine_inject_entropy(&client.eng, entropy, sizeof(entropy));
    return br_ssl_client_reset(&client, "oakos.invalid", 0) != 0 &&
        (br_ssl_engine_current_state(&client.eng) & BR_SSL_SENDREC) != 0;
}
