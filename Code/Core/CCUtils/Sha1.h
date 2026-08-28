#ifndef SHA1_H
#define SHA1_H

#include <cstddef>
#include <string>

namespace CC
{
    // SHA-1 of (data, sizeBytes) returned as a lowercase 40-character
    // hexadecimal string. Used by the AudioTracker sample library to
    // detect tampered or replaced files without re-decoding.
    //
    // SHA-1 is not collision-resistant against motivated attackers and
    // is unsuitable for security-sensitive uses. It is used here only
    // as a content fingerprint against accidental file replacement.
    std::string Sha1Hex(const void* data, size_t sizeBytes);
}

#endif // SHA1_H
