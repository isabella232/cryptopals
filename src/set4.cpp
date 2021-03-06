#include "set4.hpp"

#include <fstream>

#include "utils.hpp"
#include "crypto.hpp"
#include "random.hpp"
#include "converter.hpp"
#include "hash.hpp"
#include "log.hpp"
#include "http.hpp"
#include "stopwatch.hpp"
#include "threadpool.hpp"

struct Crypto {
    // dd if=/dev/urandom bs=1 count=16 status=none | xxd -i -c 1000
    Bytes key = { 0x22, 0x1c, 0xc1, 0x81, 0x63, 0xeb, 0x3a, 0x84, 0x68, 0x7c, 0x66, 0xf8, 0xde, 0x4e, 0x79, 0x17 };
    uint64_t nonce = 9504;
    Bytes encrypted;

    Bytes edit( const size_t& offset, const Bytes& replacement ) {
        return crypto::editAES128CTR( encrypted, offset, replacement, key, nonce );
    }

    Bytes encrypt( const Bytes& plain ) {
        encrypted = crypto::encryptAES128CTR( plain, key, nonce );
        return encrypted;
    }
};

void testReplacement() {
    Bytes key( bytes( "YELLOW SUBMARINE" ) );
    Bytes clear( bytes( "Testing text replacement" ) );
    uint64_t nonce = 12309;
    Bytes encrypted( crypto::encryptAES128CTR( clear, key, nonce ) );

    Bytes edited = crypto::editAES128CTR( encrypted, 8, bytes( "hase" ), key, nonce );
    Bytes decrypted = crypto::decryptAES128CTR( edited, key, nonce );

    Bytes expected( bytes( "Testing hase replacement" ) );
    CHECK_EQ( decrypted, expected );
}

void challenge4_25() {
    Bytes encrypted = utils::fromBase64File( "1_7.txt" );
    Bytes key( bytes( "YELLOW SUBMARINE" ) );
    Bytes plain = crypto::decryptAES128ECB( encrypted, key );

    Crypto crypto;
    Bytes stream = crypto.encrypt( plain );

    testReplacement();

    // replace secret text with zeros to get XOR stream
    Bytes replacement( stream.size(), 0 );
    Bytes replaced = crypto.edit( 0, replacement );
    Bytes decrypted = crypto::XOR( stream, replaced );

    CHECK_EQ( plain, decrypted );
}

void challenge4_26() {
    // encrypt/decrypt with secret key
    struct {
        // dd if=/dev/urandom bs=1 count=16 status=none | xxd -i -c 1000
        Bytes key = { 0x0c, 0xc8, 0xdf, 0x18, 0x31, 0x4d, 0x46, 0x03, 0x8d, 0x53, 0x65, 0x17, 0xa7, 0x56, 0x03, 0x2d };
        // dd if=/dev/urandom bs=1 count=8 status=none | od -A none -t u8
        uint64_t nonce = 14314387627995711828u;

        Bytes pack( const std::string& userdata ) const {
            std::string request = utils::generateGETRequest( userdata );
            Bytes data( request.cbegin(), request.cend() );
            Bytes encrypted = crypto::encryptAES128CTR( data, key, nonce );
            return encrypted;
        }

        std::string decrypt( const Bytes& encrypted ) const {
            Bytes decrypted = crypto::decryptAES128CTR( encrypted, key, nonce );
            std::string request( decrypted.cbegin(), decrypted.cend() );
            return request;
        }

        bool isAdmin( const Bytes& encrypted ) const {
            Bytes decrypted = crypto::decryptAES128CTR( encrypted, key, nonce );
            std::string request( decrypted.cbegin(), decrypted.cend() );
            bool admin = utils::isAdmin( request );
            return admin;
        }
    } Packer;

    std::string userdata = "|admin|true";
    Bytes encrypted = Packer.pack( userdata );
    LOG( Packer.decrypt( encrypted ).substr( 32, 16 ) );

    // search for bytes at pos1 and pos2, which will change the decrypted text from
    // |admin|true
    // to
    // ;admin=true
    size_t pos1 = 32; // position of first '|'
    size_t pos2 = 38; // position of second '|'
    bool isAdmin = false;

    for( size_t flip1 = 0; flip1 < 256; ++flip1 ) {
        for( size_t flip2 = 0; flip2 < 256; ++flip2 ) {

            encrypted[pos1] = ( Byte )flip1;
            encrypted[pos2] = ( Byte )flip2;

            if( Packer.isAdmin( encrypted ) ) {
                isAdmin = true;
                LOG( Packer.decrypt( encrypted ).substr( 32, 16 ) );
                goto stop;
            }
        }
    }

stop:
    CHECK( isAdmin );
}

void challenge4_27() {
    // encrypt/decrypt with secret key
    struct {
        // dd if=/dev/urandom bs=1 count=16 status=none | xxd -i -c 1000
        Bytes key = { 0x0c, 0xc8, 0xdf, 0x18, 0x31, 0x4d, 0x46, 0x03, 0x8d, 0x53, 0x65, 0x17, 0xa7, 0x56, 0x03, 0x2d };

        Bytes pack( const std::string& userdata ) const {
            std::string request = utils::generateGETRequest( userdata );
            Bytes data( request.cbegin(), request.cend() );
            Bytes encrypted = crypto::encryptAES128CBC( data, key, key );
            return encrypted;
        }

        std::string decrypt( const Bytes& encrypted ) const {
            Bytes decrypted = crypto::decryptAES128CBC( encrypted, key, key );
            std::string request( decrypted.cbegin(), decrypted.cend() );
            return request;
        }

        std::optional<std::string> isBad( const Bytes& encrypted ) const {
            std::string request = decrypt( encrypted );

            if( !utils::isAscii( request ) ) {
                return request;
            }

            return {};
        }
    } Packer;

    // ensure encrypted data is min 3 blocks big
    // AES-CBC(P_1, P_2, P_3) -> C_1, C_2, C_3
    std::string userdata( 3 * crypto::blockSize, 'A' );
    Bytes encrypted = Packer.pack( userdata );
    CHECK( !Packer.isBad( encrypted ) );

    // C_1, C_2, C_3 -> C_1, 0, C_1
    {
        // nullify 2nd block
        for( size_t pos = crypto::blockSize; pos < 2 * crypto::blockSize; ++pos ) {
            encrypted[pos] = 0;
        }

        // copy first to third block
        for( size_t pos = 0; pos < crypto::blockSize; ++pos ) {
            encrypted[pos + 2 * crypto::blockSize] = encrypted[pos];
        }
    }
    auto isBad = Packer.isBad( encrypted );
    CHECK( isBad );

    if( !isBad ) { return; }

    std::string error = isBad.value_or( "" );

    // P'_1 XOR P'_3
    Bytes p1( error.cbegin(), error.cbegin() + crypto::blockSize );
    Bytes p3( error.cbegin() + 2 * crypto::blockSize, error.cbegin() + 3 * crypto::blockSize );
    Bytes key = crypto::XOR( p1, p3 );
    CHECK_EQ( key, Packer.key );

}

void challenge4_28() {
    // verify sha1 code
    std::map<std::string, std::string> hashes = {
        {"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
        {"Hallo", "59d9a6df06b9f610f7db8e036896ed03662d168f"},
        {std::string( 127, 'A' ), "8c8393ac8939430753d7cb568e2f2237bc62d683"},
    };

    for( auto&& hash : hashes ) {
        Bytes data = bytes( hash.first );
        Bytes sha1 = hash::sha1( data );
        CHECK_EQ( converter::binaryToHex( sha1 ), hash.second );
    }

    // generate sha1 MAC
    Bytes key = crypto::genKey();
    Bytes message = randombuffer::get( 100 );
    Bytes mac = crypto::macSha1( message, key );

    // flip every bit of the message and verify its MAC changed
    for( size_t i = 0; i < 800; ++i ) {
        size_t pos = i / 8;
        uint8_t flip = 1 << i % 8;

        message[pos] ^= flip;
        Bytes newMac = crypto::macSha1( message, key );
        CHECK_NE( newMac, mac );

        // restore original message
        message[pos] ^= flip;
    }

    // verify, that a random generated key cannot generate the same MAC
    for( size_t i = 0; i < 1000; ++i ) {
        Bytes newKey = crypto::genKey();
        Bytes newMac = crypto::macSha1( message, newKey );
        CHECK_NE( newMac, mac );
    }
}

void challenge4_29() {
    Bytes key = { 0x22, 0xb7, 0x1a, 0x9b, 0x98, 0xf5, 0xae, 0x90, 0xac, 0xea, 0xfd, 0xee, 0x9a, 0x57, 0xe1, 0xdf };
    Bytes suffix = bytes( ";admin=true" );
    Bytes message = bytes( "comment1=cooking%20MCs;userdata=foo;comment2=%20like%20a%20pound%20of%20bacon" );
    Bytes mac = crypto::macSha1( message, key );
    LOG( "ORIG   : " << converter::binaryToHex( mac ) );

    // size of message + key is at least size of message
    size_t guess = message.size();

    for( ; guess < 1000; ++guess ) {

        Bytes padding = hash::sha1MDPadding<Bytes, true>( guess );
        Bytes attack = message + padding + suffix;
        Bytes server = crypto::macSha1( attack, key );

        hash::Magic<5> magic = hash::shaToMagics<Bytes, 5, true>( mac );
        Bytes sha = hash::magicsToSha<Bytes, 5, true>( magic );
        CHECK_EQ( mac, sha );

        Bytes fakeKey( guess - message.size(), 0 );
        size_t hashedSize = guess + padding.size();
        CHECK( ( hashedSize % 64 ) == 0 );
        Bytes forged = hash::sha1( fakeKey + attack, magic, hashedSize );

        if( forged == server ) {
            // we found a message, the server will accept
            LOG( "GUESS  : " << guess );
            LOG( "ORIG+  : " << converter::binaryToHex( server ) );
            LOG( "FORGED : " << converter::binaryToHex( forged ) );
            break;
        }
    }

    CHECK_EQ( guess, key.size() + message.size() );
}

void challenge4_30() {
    // verify md4 code
    std::map<std::string, std::string> hashes = {
        {"", "31d6cfe0d16ae931b73c59d7e0c089c0"},
        {"Hallo", "4d6e0719eea8604d62a2e62bb63f1ebd"},
        {std::string( 127, 'A' ), "dbbc89e0dff14f64313a077e1ddc5e01"},
    };

    for( auto&& hash : hashes ) {
        Bytes data = bytes( hash.first );
        Bytes md4 = hash::md4( data );
        CHECK_EQ( converter::binaryToHex( md4 ), hash.second );
    }

    // extend hash
    Bytes key = { 0x22, 0xb7, 0x1a, 0x9b, 0x98, 0xf5, 0xae, 0x90, 0xac, 0xea, 0xfd, 0xee, 0x9a, 0x57, 0xe1, 0xdf };
    Bytes suffix = bytes( ";admin=true" );
    Bytes message = bytes( "comment1=cooking%20MCs;userdata=foo;comment2=%20like%20a%20pound%20of%20bacon" );
    Bytes mac = crypto::macMd4( message, key );
    LOG( "ORIG   : " << converter::binaryToHex( mac ) );

    // size of message + key is at least size of message
    size_t guess = message.size();

    for( ; guess < 1000; ++guess ) {

        Bytes padding = hash::sha1MDPadding<Bytes, false>( guess );
        Bytes attack = message + padding + suffix;
        Bytes server = crypto::macMd4( attack, key );

        hash::Magic<4> magic = hash::shaToMagics<Bytes, 4, false>( mac );
        Bytes sha = hash::magicsToSha<Bytes, 4, false>( magic );
        CHECK_EQ( mac, sha );

        Bytes fakeKey( guess - message.size(), 0 );
        size_t hashedSize = guess + padding.size();
        CHECK( ( hashedSize % 64 ) == 0 );
        Bytes forged = hash::md4( fakeKey + attack, magic, hashedSize );

        if( forged == server ) {
            // we found a message, the server will accept
            LOG( "GUESS  : " << guess );
            LOG( "ORIG+  : " << converter::binaryToHex( server ) );
            LOG( "FORGED : " << converter::binaryToHex( forged ) );
            break;
        }
    }

    CHECK_EQ( guess, key.size() + message.size() );
}

// view with
// xmgrace -legend load times_*
void dump( const std::string& filename, const std::array<size_t, 256>& times ) {
    std::ofstream of( filename, std::ios::out | std::ios::binary | std::ios::app );

    for( size_t i = 0; i < times.size(); ++i ) {
        of << i << " " << ( times[i] / 1000000 ) << std::endl;
    }
}

Bytes guessHash( const std::string& path, const size_t iters = 1, const size_t threads = 4 ) {
    // sha1 guess
    Threadpool pool( threads );
    std::string expected = "fa9908c7e2e1dfe6917b19ccfc04998ead09aef9";
    Bytes guess( 20, 0 );

    for( size_t pos = 0; pos < guess.size(); ++pos ) {

        std::string filename = utils::format( "times_%02i.txt", pos );
        std::remove( filename.c_str() );

        std::array<size_t, 256> times;

        for( size_t i = 0; i < 256; ++i ) {
            pool.add( [&, pos, i] {

                Bytes copy = guess;
                // warm up run
                http::GET( path + converter::binaryToHex( copy ) );
                copy[pos] = i;

                StopWatch watch;
                watch.start();

                for( auto&& ab : Bytes( iters ) ) {
                    ( void )ab;
                    http::GET( path + converter::binaryToHex( copy ) );
                }

                auto ns = watch.stop();
                times[i] = ns;
            } );
        }

        pool.waitForJobs();

        // log to file to view with xmgrace
        dump( filename, times );

        // find largest duration
        auto iter = std::max_element( times.cbegin(), times.cend() );
        size_t best = iter - times.cbegin();

        guess[pos] = best;
        LOG_DEBUG( "B : " << best );
        LOG_DEBUG( "T : " << ( *iter / 1000000 ) );
        std::string current = converter::binaryToHex( guess ).substr( 0, 2 * ( pos + 1 ) );
        LOG( current );

        if( expected.find( current ) == std::string::npos ) {
            LOG( "Guess is off at pos " << pos );
            return guess;
        }
    }

    CHECK_EQ( expected, converter::binaryToHex( guess ) );
    return guess;
}

void challenge4_31() {
    // check hmac
    // https://caligatio.github.io/jsSHA/
    {
        Bytes key( bytes( "YELLOW SUBMARINE" ) );
        Bytes text( bytes( "Text, which will be hmac'ed" ) );
        Bytes expected = converter::hexToBinary( "ad04d03f084ce2c18ab48ca350c08513ea08caeb" );
        CHECK_EQ( crypto::hmacSha1( text, key ), expected );
    }

    std::string prefix = "http://localhost:9000/test?file=foo&signature=";

    // check server connection
    {
        if( !http::GET( "http://localhost:9000/ok" ) ) {
            LOG( "./server.py 9000 2> /dev/null" );
            return;
        }

        int status = http::GET( prefix + "46b4ec586117154dacd49d664e5d63fdc88efb51" );
        CHECK_EQ( status, 500 );

        int status2 = http::GET( prefix + "fa9908c7e2e1dfe6917b19ccfc04998ead09aef9" );
        CHECK_EQ( status2, 200 );
    }

    Bytes guess = guessHash( prefix );
    int status = http::GET( prefix + converter::binaryToHex( guess ) );
    CHECK_EQ( status, 200 );
}

void challenge4_32() {

    // check server connection
    if( !http::GET( "http://localhost:9000/ok" ) ) {
        LOG( "./server.py 9000 2> /dev/null" );
        return;
    }

    std::string prefix = "http://localhost:9000/short?file=foo&signature=";
    // run with 10 guesses and single threaded for reliable results
    Bytes guess = guessHash( prefix, 10, 1 );
    int status = http::GET( prefix + converter::binaryToHex( guess ) );
    CHECK_EQ( status, 200 );
}
