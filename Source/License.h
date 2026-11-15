#pragma once

#include "Common.h"

namespace brl::License
{
    struct Info
    {
        bool   valid = false;
        String email, orderId;
    };

    /** Payload prefix inside every signed key. */
    inline constexpr const char* kPayloadPrefix = "BRL1|";

    /** Verifies a key offline against the public key compiled into the plugin. */
    Info verify (const String& key);

    /** Reads the saved key (if any) and verifies it. */
    Info current();

    /** Verifies and stores a key. Returns an error message, or empty on success. */
    String activate (const String& key);

    void deactivate();

    /** Seller-side helpers (used by brl_keygen only). */
    String sign (const RSAKey& privateKey, const String& email, const String& orderId);
    Info   verifyWith (const RSAKey& publicKey, const String& key);
}
