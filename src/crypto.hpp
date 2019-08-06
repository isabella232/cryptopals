#pragma once

#include "utils.hpp"

namespace crypto {
const size_t blockSize = 16;

std::string XOR( const std::string& first, const std::string& second );
Bytes XOR( const Bytes& first, const Bytes& second );
Bytes XOR( const Bytes& first, const uint8_t& key );

Bytes encryptAES128ECB( const Bytes& text, const Bytes& key );
Bytes decryptAES128ECB( const Bytes& data, const Bytes& key );

Bytes encryptAES128CBC( const Bytes& text, const Bytes& key, const Bytes& iv );
Bytes decryptAES128CBC( const Bytes& data, const Bytes& key, const Bytes& iv );

//! pad \p input in PKCS#7 to \p size bytes
template <class Container>
Container padPKCS7( const Container& input, const size_t blockSize = crypto::blockSize );

template <class Container>
Container unpadPKCS7( const Container& input );

Bytes genKey();
Bytes randBytes( const size_t& size );
size_t randSize( const size_t& from, const size_t& to );
bool flipCoin();

// encrypts \p data randomly with CBC or ECB with a generated Key
struct Encrypted {
    enum class Type { ECB, CBC };
    Type type;
    Bytes bytes;
};
Bytes encryptECBWithSecretSuffix( const Bytes& data );
Bytes encryptECBWithRandomPrefixAndSecretSuffix( const Bytes& data );
Encrypted encryptECBOrCBC( const Bytes& data );
std::ostream& operator<<( std::ostream& os, const Encrypted::Type& type );

}
