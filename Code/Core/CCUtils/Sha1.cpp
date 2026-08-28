#include "Sha1.h"

#include <cstdint>
#include <cstring>

namespace CC
{
    static const int SHA1_BLOCK_SIZE_BYTES  = 64;
    static const int SHA1_DIGEST_SIZE_BYTES = 20;

    static uint32_t LeftRotate(uint32_t value, int bitCount)
    {
        return (value << bitCount) | (value >> (32 - bitCount));
    }

    // Processes one 512-bit (64-byte) block, mutating the running state
    // h0..h4 in place. The block is interpreted as 16 big-endian 32-bit
    // words, expanded to 80 words, then mixed in four rounds of 20.
    static void Sha1ProcessBlock(uint32_t state[5], const uint8_t block[SHA1_BLOCK_SIZE_BYTES])
    {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
        {
            w[i] = ((uint32_t)block[i * 4 + 0] << 24)
                 | ((uint32_t)block[i * 4 + 1] << 16)
                 | ((uint32_t)block[i * 4 + 2] <<  8)
                 | ((uint32_t)block[i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++)
        {
            w[i] = LeftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];

        for (int i = 0; i < 80; i++)
        {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }

            uint32_t temp = LeftRotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = LeftRotate(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

    std::string Sha1Hex(const void* data, size_t sizeBytes)
    {
        uint32_t state[5];
        state[0] = 0x67452301u;
        state[1] = 0xEFCDAB89u;
        state[2] = 0x98BADCFEu;
        state[3] = 0x10325476u;
        state[4] = 0xC3D2E1F0u;

        const uint8_t* bytes        = (const uint8_t*)data;
        size_t         remaining    = sizeBytes;
        uint64_t       totalBitCount = (uint64_t)sizeBytes * 8u;

        while (remaining >= (size_t)SHA1_BLOCK_SIZE_BYTES)
        {
            Sha1ProcessBlock(state, bytes);
            bytes     += SHA1_BLOCK_SIZE_BYTES;
            remaining -= (size_t)SHA1_BLOCK_SIZE_BYTES;
        }

        // Final block: copy the trailing bytes, append 0x80 marker,
        // pad with zeros, append the 64-bit big-endian message length.
        // If the marker + length don't fit in one block, emit two.
        uint8_t finalBlocks[SHA1_BLOCK_SIZE_BYTES * 2];
        std::memset(finalBlocks, 0, sizeof(finalBlocks));
        std::memcpy(finalBlocks, bytes, remaining);
        finalBlocks[remaining] = 0x80;

        size_t totalLength = (remaining + 1 <= 56) ? SHA1_BLOCK_SIZE_BYTES : (SHA1_BLOCK_SIZE_BYTES * 2);
        for (int i = 0; i < 8; i++)
        {
            finalBlocks[totalLength - 1 - i] = (uint8_t)(totalBitCount >> (i * 8));
        }

        Sha1ProcessBlock(state, finalBlocks);
        if (totalLength == (size_t)(SHA1_BLOCK_SIZE_BYTES * 2))
        {
            Sha1ProcessBlock(state, finalBlocks + SHA1_BLOCK_SIZE_BYTES);
        }

        char hexDigits[SHA1_DIGEST_SIZE_BYTES * 2 + 1];
        const char* hexAlphabet = "0123456789abcdef";
        for (int i = 0; i < 5; i++)
        {
            for (int byteIndex = 0; byteIndex < 4; byteIndex++)
            {
                uint8_t value = (uint8_t)(state[i] >> ((3 - byteIndex) * 8));
                hexDigits[(i * 4 + byteIndex) * 2 + 0] = hexAlphabet[(value >> 4) & 0xF];
                hexDigits[(i * 4 + byteIndex) * 2 + 1] = hexAlphabet[ value       & 0xF];
            }
        }
        hexDigits[SHA1_DIGEST_SIZE_BYTES * 2] = '\0';

        return std::string(hexDigits);
    }
}
