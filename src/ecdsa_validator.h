#pragma once

#include <string>

namespace ohboyscan {

// Feature 1: ECDSA (FIPS 186-5, P-256 + SHA-256) signature verification for
// RouterOS script integrity, built on OpenSSL.
class EcdsaValidator {
public:
    // Verifies an ECDSA P-256 signature (r and s as hex) over `message` with
    // an uncompressed SEC1 public key (0x04 || X || Y) in hex.
    static bool verify(const std::string& message, const std::string& r_hex,
                       const std::string& s_hex,
                       const std::string& public_key_hex);

    // Generates a throwaway P-256 key, signs and verifies a test message
    // through the public verify() path. Returns true when the engine works.
    static bool self_test();
};

} // namespace ohboyscan
