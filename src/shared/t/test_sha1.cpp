#include <shared/system.h>
#include <shared/sha1.h>
#include <shared/testing.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define TEST_8GB 1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t DATA[] = {
#include "sha1_test_data.dat.txt"
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    {
        static const uint8_t WANTED_DIGEST[] = {
            0xf5,
            0x11,
            0xfb,
            0x28,
            0x85,
            0xff,
            0xec,
            0x37,
            0xf1,
            0x4c,
            0x70,
            0xe6,
            0x79,
            0xce,
            0xb0,
            0x05,
            0x70,
            0xd7,
            0xa0,
            0x12,
        };
        uint8_t digest[20];
        char digest_str[41];

        SHA1::HashBuffer(digest, digest_str, DATA, sizeof DATA);

        TEST_EQ_SS(digest_str, "f511fb2885ffec37f14c70e679ceb00570d7a012");
        TEST_EQ_AA(digest, WANTED_DIGEST, 20);
    }

#if TEST_8GB
    /* Check the thing works properly with data substantially over the
     * 32-bit limit. I was a little suspicious about this. (It takes an
     * age to run, so it's disabled by default.) */
    {
        size_t size = (size_t)8 * 1024 * 1024 * 1024;
        size_t num_copies = 0;
        uint8_t *buf = malloc(size);
        uint8_t *end = buf + size;
        uint8_t *dest = buf;
        char digest_str[41];

        while (dest < end) {
            size_t n = sizeof DATA;

            if (dest + n >= end) {
                n = end - dest;
            }

            memcpy(dest, DATA, n);
            dest += sizeof DATA;
            ++num_copies;
        }

        SHA1::HashBuffer(NULL, digest_str, buf, size);

        TEST_EQ_SS(digest_str, "58911bc1752ca2b4a452c56ec85a666aa9b42d5a");

        free(buf);
    }
#endif

    return 0;
}
