#pragma once

#include <vector>
#include <string>

#include "log.hpp"

#define CHECK( A ) if( !(A) ) { LOG( "Failed check for "#A ); }
#define CHECK_EQ( A, B ) if( (A) != (B) ) { LOG( "Failed check for "#A" == "#B", " << A << " != " << B ); }

using Bytes = std::vector<uint8_t>;
using Byte = uint8_t;

namespace utils {

uint8_t parseHex( const char& hex );
Bytes hexToBinary( const std::string& hex );
std::string binaryToHex( const Bytes& bytes );

std::string binaryToBase64( const Bytes& binary );
std::string hexToBase64( const std::string& hex );

std::string XOR( const std::string& first, const std::string& second );
Bytes XOR( const Bytes& first, const Bytes& second );
Bytes XOR( const Bytes& first, const uint8_t& key );

float isEnglishText( const Bytes& text );

}
