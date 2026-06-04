#line 1 "D:\\Power_Central_v4\\totp_auth.cpp"
#include "totp_auth.h"

#include <ctype.h>
#include <esp_system.h>
#include <mbedtls/md.h>

namespace
{
    constexpr uint8_t kTotpDigits = 6;
    constexpr uint32_t kTotpPeriodSeconds = 30;
    constexpr size_t kSecretBytes = 20;
    constexpr size_t kSha1DigestBytes = 20;
    const char *kBase32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    int base32Value(char c)
    {
        c = (char)toupper((unsigned char)c);
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= '2' && c <= '7')
            return c - '2' + 26;
        return -1;
    }

    bool base32Encode(const uint8_t *input, size_t inputLen, char *dest, size_t destSize)
    {
        if (!input || !dest || destSize == 0)
            return false;

        uint32_t buffer = 0;
        uint8_t bitsLeft = 0;
        size_t outLen = 0;

        for (size_t i = 0; i < inputLen; i++)
        {
            buffer = (buffer << 8) | input[i];
            bitsLeft += 8;
            while (bitsLeft >= 5)
            {
                if (outLen + 1 >= destSize)
                    return false;
                dest[outLen++] = kBase32Alphabet[(buffer >> (bitsLeft - 5)) & 0x1F];
                bitsLeft -= 5;
            }
        }

        if (bitsLeft > 0)
        {
            if (outLen + 1 >= destSize)
                return false;
            dest[outLen++] = kBase32Alphabet[(buffer << (5 - bitsLeft)) & 0x1F];
        }

        dest[outLen] = '\0';
        return true;
    }

    bool base32Decode(const char *input, uint8_t *dest, size_t destSize, size_t &outLen)
    {
        if (!input || !dest)
            return false;

        uint32_t buffer = 0;
        uint8_t bitsLeft = 0;
        outLen = 0;

        for (const char *p = input; *p; p++)
        {
            if (*p == ' ' || *p == '-' || *p == '=')
                continue;

            const int value = base32Value(*p);
            if (value < 0)
                return false;

            buffer = (buffer << 5) | (uint32_t)value;
            bitsLeft += 5;
            if (bitsLeft >= 8)
            {
                if (outLen >= destSize)
                    return false;
                dest[outLen++] = (uint8_t)((buffer >> (bitsLeft - 8)) & 0xFF);
                bitsLeft -= 8;
            }
        }

        return outLen > 0;
    }

    uint32_t pow10u(uint8_t digits)
    {
        uint32_t value = 1;
        while (digits--)
            value *= 10;
        return value;
    }

    bool makeTotpAtStep(const uint8_t *key, size_t keyLen, uint64_t timeStep, char outCode[kTotpDigits + 1])
    {
        uint8_t msg[8];
        for (int i = 7; i >= 0; i--)
        {
            msg[i] = (uint8_t)(timeStep & 0xFF);
            timeStep >>= 8;
        }

        uint8_t digest[kSha1DigestBytes];
        const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
        if (!mdInfo)
            return false;

        if (mbedtls_md_hmac(mdInfo, key, keyLen, msg, sizeof(msg), digest) != 0)
            return false;

        const uint8_t offset = digest[kSha1DigestBytes - 1] & 0x0F;
        const uint32_t binary =
            ((uint32_t)(digest[offset] & 0x7F) << 24) |
            ((uint32_t)digest[offset + 1] << 16) |
            ((uint32_t)digest[offset + 2] << 8) |
            (uint32_t)digest[offset + 3];
        const uint32_t otp = binary % pow10u(kTotpDigits);

        snprintf(outCode, kTotpDigits + 1, "%06lu", (unsigned long)otp);
        return true;
    }

    bool fixedTimeEquals(const char *a, const char *b, size_t len)
    {
        uint8_t diff = 0;
        for (size_t i = 0; i < len; i++)
            diff |= (uint8_t)(a[i] ^ b[i]);
        return diff == 0;
    }

    String urlEncode(const char *text)
    {
        String out;
        if (!text)
            return out;

        const char hex[] = "0123456789ABCDEF";
        while (*text)
        {
            const uint8_t c = (uint8_t)*text++;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            {
                out += (char)c;
            }
            else
            {
                out += '%';
                out += hex[(c >> 4) & 0x0F];
                out += hex[c & 0x0F];
            }
        }
        return out;
    }
}

bool generateTotpSecret(char *dest, size_t destSize)
{
    uint8_t randomBytes[kSecretBytes];
    for (size_t i = 0; i < kSecretBytes; i += 4)
    {
        uint32_t value = esp_random();
        for (size_t j = 0; j < 4 && i + j < kSecretBytes; j++)
        {
            randomBytes[i + j] = (uint8_t)(value & 0xFF);
            value >>= 8;
        }
    }

    return base32Encode(randomBytes, sizeof(randomBytes), dest, destSize);
}

bool isTotpCodeFormat(const char *code)
{
    if (!code)
        return false;

    for (uint8_t i = 0; i < kTotpDigits; i++)
    {
        if (!isdigit((unsigned char)code[i]))
            return false;
    }

    return code[kTotpDigits] == '\0';
}

bool verifyTotpCode(const char *base32Secret, const char *code, uint32_t unixTime, uint8_t windowSteps)
{
    if (!base32Secret || !base32Secret[0] || !isTotpCodeFormat(code) || unixTime < kTotpPeriodSeconds)
        return false;

    uint8_t key[32];
    size_t keyLen = 0;
    if (!base32Decode(base32Secret, key, sizeof(key), keyLen))
        return false;

    const uint64_t currentStep = (uint64_t)unixTime / kTotpPeriodSeconds;
    for (int8_t offset = -(int8_t)windowSteps; offset <= (int8_t)windowSteps; offset++)
    {
        if (offset < 0 && currentStep < (uint64_t)(-offset))
            continue;

        const uint64_t candidateStep = offset < 0
                                           ? currentStep - (uint64_t)(-offset)
                                           : currentStep + (uint64_t)offset;

        char expected[kTotpDigits + 1];
        if (!makeTotpAtStep(key, keyLen, candidateStep, expected))
            return false;

        if (fixedTimeEquals(expected, code, kTotpDigits))
            return true;
    }

    return false;
}

String buildTotpUri(const char *issuer, const char *account, const char *base32Secret)
{
    String safeIssuer = urlEncode(issuer);
    String safeAccount = urlEncode(account);
    String uri = "otpauth://totp/";
    uri += safeIssuer;
    uri += ":";
    uri += safeAccount;
    uri += "?secret=";
    uri += base32Secret;
    uri += "&issuer=";
    uri += safeIssuer;
    uri += "&digits=6&period=30";
    return uri;
}
