#define OPENSSL_SUPPRESS_DEPRECATED

#include "ecdsa_validator.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ohboyscan {
namespace {

using BigNumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using EcKeyPtr = std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)>;
using EcSigPtr = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::vector<std::uint8_t> hex_decode(const std::string& hex) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        const int hi = hex_nibble(hex[i]);
        const int lo = hex_nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return {};
        }
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

std::string to_hex(const std::uint8_t* data, std::size_t length) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        out.push_back(kDigits[data[i] >> 4]);
        out.push_back(kDigits[data[i] & 0x0F]);
    }
    return out;
}

BigNumPtr bn_from_hex(const std::string& hex) {
    auto bytes = hex_decode(hex);
    if (bytes.empty()) {
        return {nullptr, BN_free};
    }
    return {BN_bin2bn(bytes.data(), static_cast<int>(bytes.size()), nullptr), BN_free};
}

std::string bn_to_hex(const BIGNUM* bn) {
    char* hex = BN_bn2hex(bn);
    if (hex == nullptr) {
        return {};
    }
    std::string out(hex);
    OPENSSL_free(hex);
    return out;
}

} // namespace

bool EcdsaValidator::verify(const std::string& message, const std::string& r_hex,
                            const std::string& s_hex,
                            const std::string& public_key_hex) {
    auto r = bn_from_hex(r_hex);
    auto s = bn_from_hex(s_hex);
    if (!r || !s) {
        return false;
    }

    EcSigPtr signature(ECDSA_SIG_new(), ECDSA_SIG_free);
    if (!signature) {
        return false;
    }
    if (ECDSA_SIG_set0(signature.get(), r.release(), s.release()) != 1) {
        return false;
    }

    auto key_bytes = hex_decode(public_key_hex);
    if (key_bytes.empty()) {
        return false;
    }

    EcKeyPtr ec(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), EC_KEY_free);
    if (!ec) {
        return false;
    }

    EC_KEY* raw_ec = ec.release();
    const auto* cursor = key_bytes.data();
    if (o2i_ECPublicKey(&raw_ec, &cursor, static_cast<long>(key_bytes.size())) == nullptr) {
        EC_KEY_free(raw_ec);
        return false;
    }
    ec.reset(raw_ec);

    PkeyPtr pkey(EVP_PKEY_new(), EVP_PKEY_free);
    if (!pkey) {
        return false;
    }
    if (EVP_PKEY_set1_EC_KEY(pkey.get(), ec.get()) != 1) {
        return false;
    }

    unsigned char* der = nullptr;
    const int der_length = i2d_ECDSA_SIG(signature.get(), &der);
    if (der_length <= 0) {
        return false;
    }

    bool ok = false;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx != nullptr) {
        ok = EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey.get()) == 1 &&
             EVP_DigestVerifyUpdate(ctx, message.data(), message.size()) == 1 &&
             EVP_DigestVerifyFinal(ctx, der, der_length) == 1;
        EVP_MD_CTX_free(ctx);
    }
    OPENSSL_free(der);
    return ok;
}

bool EcdsaValidator::self_test() {
    EVP_PKEY_CTX* key_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (key_ctx == nullptr) {
        return false;
    }

    EVP_PKEY* raw_key = nullptr;
    bool ok = EVP_PKEY_keygen_init(key_ctx) == 1 &&
              EVP_PKEY_CTX_set_ec_paramgen_curve_nid(key_ctx, NID_X9_62_prime256v1) == 1 &&
              EVP_PKEY_keygen(key_ctx, &raw_key) == 1;
    EVP_PKEY_CTX_free(key_ctx);
    if (!ok) {
        return false;
    }

    PkeyPtr pkey(raw_key, EVP_PKEY_free);

    const std::string message = "OhBoyScan ECDSA self-test";

    EVP_MD_CTX* sign_ctx = EVP_MD_CTX_new();
    if (sign_ctx == nullptr) {
        return false;
    }

    std::size_t signature_length = 0;
    ok = EVP_DigestSignInit(sign_ctx, nullptr, EVP_sha256(), nullptr, pkey.get()) == 1 &&
         EVP_DigestSignUpdate(sign_ctx, message.data(), message.size()) == 1 &&
         EVP_DigestSignFinal(sign_ctx, nullptr, &signature_length) == 1;

    std::vector<std::uint8_t> der(signature_length);
    if (ok) {
        ok = EVP_DigestSignFinal(sign_ctx, der.data(), &signature_length) == 1;
    }
    EVP_MD_CTX_free(sign_ctx);
    if (!ok) {
        return false;
    }

    const auto* der_cursor = der.data();
    EcSigPtr signature(
        d2i_ECDSA_SIG(nullptr, &der_cursor, static_cast<long>(der.size())),
        ECDSA_SIG_free);
    if (!signature) {
        return false;
    }

    const EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey.get());
    if (ec == nullptr) {
        return false;
    }

    unsigned char* public_bytes = nullptr;
    const int public_length = i2o_ECPublicKey(ec, &public_bytes);
    if (public_length <= 0) {
        return false;
    }
    const std::string public_hex =
        to_hex(public_bytes, static_cast<std::size_t>(public_length));
    OPENSSL_free(public_bytes);

    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(signature.get(), &r, &s);
    if (r == nullptr || s == nullptr) {
        return false;
    }

    return verify(message, bn_to_hex(r), bn_to_hex(s), public_hex);
}

} // namespace ohboyscan
