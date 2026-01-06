/************************************************************************************************
 *
 *  _   _      _                      _
 * | \ | | ___| |___      _____  _ __| | __
 * |  \| |/ _ \ __\ \ /\ / / _ \| '__| |/ /
 * | |\  |  __/ |_ \ V  V / (_) | |  |   <
 * |_| \_|\___|\__| \_/\_/ \___/|_|  |_|\_\
 *
 * @author ALVES Quentin
 * @license MIT
 *
 ***********************************************************************************************/

#include "net_utils.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
// HELPER
/////////////////////////////////////////////////////////////////////////////////////////////////
void net_print_error( const char *format, ... ) {
    va_list args;

    va_start( args, format );
    vfprintf( stderr, format, args );
    va_end( args );

    fprintf( stderr, "\n%s\n", strerror( errno ) );
}

uint32_t parse_uint32( char* string ) {
    if ( string == NULL )
        return 0;
    
    return strtoul( string, NULL, 10 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// BUFFER
/////////////////////////////////////////////////////////////////////////////////////////////////
net_buffer_t net_buffer_immutable( const char* text ) {
    net_buffer_t buffer;

    buffer.length = strlen( text );
    buffer.size   = buffer.length;
    buffer.data   = (void*)text;

    return buffer;
}

net_buffer_t net_buffer_reference( const net_buffer_t* other, const uint32_t offset ) {
    assert( net_buffer_is_valid( other ) == enet_true );

    net_buffer_t buffer;

    buffer.length = other->length;

    if ( other->size > 0 )
        buffer.size = other->size - offset;
    else 
        buffer.size = buffer.length;

    buffer.data   = net_buffer_get_raw( other ) + offset;

    return buffer;
}

enet_booleans net_buffer_create( net_buffer_t* buffer, uint32_t length ) {
    assert( buffer != NULL );
    assert( length > 0 );

    buffer->size = 0;

    if ( net_buffer_is_valid( buffer ) == enet_false ) {
        buffer->length = length;
        buffer->data = malloc( length );
    } else if ( buffer->length < length ) {
        buffer->length = length;
        buffer->data = realloc( buffer->data, length );
    }

    if ( buffer->data != NULL )
        return enet_true;

    net_print_error( "Can't allocate %u bytes to create buffer %p", length, buffer );

    return enet_false;
}

enet_booleans net_buffer_resize( net_buffer_t* buffer, const uint32_t size ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    if ( size <= buffer->length ) {
        buffer->size = size;

        return enet_true;
    }

    return enet_false;
}

void net_buffer_clear( net_buffer_t* buffer ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    memset( buffer->data, 0x00, buffer->length );
}

enet_booleans net_buffer_is_valid( const net_buffer_t* buffer ) {
    if ( buffer == NULL || buffer->length == 0 || buffer->data == NULL )
        return enet_false;

    return enet_true;
}

enet_booleans net_buffer_is_full( const net_buffer_t* buffer ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    if ( buffer->size < buffer->length )
        return enet_false;
    
    return enet_true;
}

enet_booleans net_buffer_is_empty( const net_buffer_t* buffer ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    if ( buffer->size > 0 )
        return enet_false;

    return enet_true;
}

enet_booleans net_buffer_contain( const net_buffer_t* buffer, const char* string ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );
    assert( string != NULL );

    const uint32_t length = (uint32_t)strlen( string );

    if ( length == 0 )
        return enet_false;

    if ( memmem( buffer->data, buffer->size, string, length ) != NULL )
        return enet_true;

    return enet_false;
}

uint8_t* net_buffer_get_raw( const net_buffer_t* buffer ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    return (uint8_t*)buffer->data;
}

void net_buffer_destroy( net_buffer_t* buffer ) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    free( buffer->data );
    memset( buffer, 0x00, sizeof(net_buffer_t) );
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// BUFFER IO
/////////////////////////////////////////////////////////////////////////////////////////////////
net_buffer_io_t net_buffer_io_acquire(
    net_buffer_t* buffer,
    const enet_buffer_io_modes mode
) {
    assert( net_buffer_is_valid( buffer ) == enet_true );

    net_buffer_io_t buffer_io;
    memset( &buffer_io, 0x00, sizeof(net_buffer_io_t) );

    buffer_io.mode   = mode;
    buffer_io.buffer = buffer;
    buffer_io.head   = 0;

    return buffer_io;
}

void net_buffer_io_reset( net_buffer_io_t* buffer_io ) {
    assert( net_buffer_io_is_valid( buffer_io ) == enet_true );

    buffer_io->head = 0;

    if ( net_buffer_io_can( buffer_io, enet_buffer_io_write ) )
        net_buffer_resize( buffer_io->buffer, 0 );
}

enet_booleans net_buffer_io_read( net_buffer_io_t* buffer_io, void* out, const size_t size ) {
    assert( buffer_io != NULL );
    assert( out != NULL );
    assert( size > 0 );

    if ( net_buffer_io_can( buffer_io, enet_buffer_io_read ) == enet_false )
        return enet_false;

    const uint8_t* src_buffer = (const uint8_t*)net_buffer_get_raw( buffer_io->buffer ) + buffer_io->head;

    memmove( out, src_buffer, size );

    return net_buffer_io_jump( buffer_io, size );
}

enet_booleans net_buffer_io_read_int8( net_buffer_io_t* buffer_io, int8_t* out ) {
    return net_buffer_io_read( buffer_io, out, sizeof(int8_t) );
}

enet_booleans net_buffer_io_read_int16( net_buffer_io_t* buffer_io, int16_t* out ) {
    if ( net_buffer_io_read( buffer_io, out, sizeof(int16_t) ) == enet_false )
        return enet_false;

    (*out) = be16toh( *out );

    return enet_true;
}

enet_booleans net_buffer_io_read_int32( net_buffer_io_t* buffer_io, int32_t* out ) {
    if ( net_buffer_io_read( buffer_io, out, sizeof(int32_t) ) == enet_false )
        return enet_false;

    (*out) = be32toh( *out );

    return enet_true;
}

enet_booleans net_buffer_io_read_int64( net_buffer_io_t* buffer_io, int64_t* out ) {
    if ( net_buffer_io_read( buffer_io, out, sizeof( int64_t ) ) == enet_false )
        return enet_false;

    (*out) = be64toh( *out );

    return enet_true;
}

enet_booleans net_buffer_io_read_uint8( net_buffer_io_t* buffer_io, uint8_t* out ) {
    return net_buffer_io_read( buffer_io, out, sizeof( uint8_t ) );
}

enet_booleans net_buffer_io_read_uint16( net_buffer_io_t* buffer_io, uint16_t* out ) {
    if ( net_buffer_io_read( buffer_io, out, sizeof( uint16_t ) ) == enet_false )
        return enet_false;

    (*out) = be16toh( *out );

    return enet_true;
}

enet_booleans net_buffer_io_read_uint32( net_buffer_io_t* buffer_io, uint32_t* out ) {
    if ( net_buffer_io_read( buffer_io, out, sizeof( uint32_t ) ) == enet_false )
        return enet_false;

    (*out) = be32toh( *out );

    return enet_true;
}

enet_booleans net_buffer_io_read_uint64( net_buffer_io_t* buffer_io, uint64_t* out ) {
    if ( net_buffer_io_read( buffer_io, out, sizeof(uint64_t) ) == enet_false )
        return enet_false;

    (*out) = be64toh(*out);

    return enet_true;
}

enet_booleans net_buffer_io_read_raw(
    net_buffer_io_t* buffer_io,
    char* out,
    const size_t out_length,
    size_t* out_remaining
) {
    assert( buffer_io != NULL );
    assert( net_buffer_is_valid( buffer_io->buffer ) == enet_true );
    assert( out != NULL );
    assert( out_length > 0 );

    size_t remaining = buffer_io->buffer->size - buffer_io->head;
    size_t length = out_length;

    if ( remaining < length ) {
        length = remaining;

        if ( out_remaining != NULL )
            (*out_remaining) = out_length - length;
    } else if ( out_remaining != NULL )
        (*out_remaining) = 0;

    return net_buffer_io_read( buffer_io, out, sizeof(char) * length );
}

enet_booleans net_buffer_io_write(
    net_buffer_io_t* buffer_io,
    const void* in,
    const size_t size
) {
    assert( buffer_io != NULL );
    assert( in != NULL );
    assert( size > 0 );

    const uint32_t remaining = buffer_io->buffer->length - buffer_io->buffer->size;

    if ( net_buffer_io_can( buffer_io, enet_buffer_io_write ) == enet_false || size > remaining )
        return enet_false;

    uint8_t *dst_buffer = net_buffer_get_raw( buffer_io->buffer ) + buffer_io->buffer->size;

    memmove( dst_buffer, in, size );

    buffer_io->buffer->size += size;

    return enet_true;
}

enet_booleans net_buffer_io_write_int8( net_buffer_io_t* buffer_io, const int8_t value ) {
    return net_buffer_io_write( buffer_io, (const void*)&value, sizeof(int8_t ) );
}

enet_booleans net_buffer_io_write_int16( net_buffer_io_t *buffer_io, const int16_t value ) {
    const int16_t tmp_val = htobe16( value );

    return net_buffer_io_write( buffer_io, (const void*)&tmp_val, sizeof( int16_t ) );
}

enet_booleans net_buffer_io_write_int32(
    net_buffer_io_t* buffer_io,
    const int32_t value
) {
    const int32_t tmp_val = htobe32( value );

    return net_buffer_io_write( buffer_io, (const void*)&tmp_val, sizeof(int32_t) );
}

enet_booleans net_buffer_io_write_int64(
    net_buffer_io_t* buffer_io,
    const int64_t value
) {
    const int64_t tmp_val = htobe64( value );

    return net_buffer_io_write( buffer_io, (const void*)&tmp_val, sizeof(int64_t) );
} 

enet_booleans net_buffer_io_write_uint8(
    net_buffer_io_t* buffer_io,
    const uint8_t value
) {
    return net_buffer_io_write( buffer_io, (const void*)&value, sizeof(uint8_t) );
}

enet_booleans net_buffer_io_write_uint16(
    net_buffer_io_t* buffer_io,
    const uint16_t value
) {
    const uint16_t tmp_val = htobe16( value );

    return net_buffer_io_write( buffer_io, (const void*)&tmp_val, sizeof(uint16_t) );
}

enet_booleans net_buffer_io_write_uint32(
    net_buffer_io_t* buffer_io,
    const uint32_t value
) {
    const uint32_t tmp_val = htobe32( value );

    return net_buffer_io_write( buffer_io, (const void*)&tmp_val, sizeof(uint32_t) );
}

enet_booleans net_buffer_io_write_uint64(
    net_buffer_io_t* buffer_io,
    const uint64_t value
) {
    const uint64_t tmp_val = htobe64( value );

    return net_buffer_io_write( buffer_io, (const void*)&tmp_val, sizeof(uint64_t) );
}

enet_booleans net_buffer_io_write_raw(
    net_buffer_io_t* buffer_io,
    const char* in,
    const size_t in_length,
    size_t* in_remaining
) {
    assert( buffer_io != NULL );
    assert( net_buffer_is_valid( buffer_io->buffer ) == enet_true );
    assert( in != NULL );
    assert( in_length > 0 );

    size_t remaining = buffer_io->buffer->size - buffer_io->head;
    size_t length = in_length;

    if ( remaining < length ) {
        length = remaining;

        if ( in_remaining != NULL )
            (*in_remaining) = in_length - length;
    } else if ( in_remaining != NULL )
        (*in_remaining) = 0;

    return net_buffer_io_write( buffer_io, in, sizeof(char) * length );
}

enet_booleans net_buffer_io_jump( net_buffer_io_t* buffer_io, const uint32_t length ) {
    assert( buffer_io );

    uint32_t offset = length;

    if ( buffer_io->head + offset > buffer_io->buffer->size )
        offset = buffer_io->buffer->size - buffer_io->head;

    if ( offset == 0 )
        return enet_false;

    buffer_io->head += length;

    return enet_true;
}

enet_booleans net_buffer_io_is_valid( const net_buffer_io_t* buffer_io ) {
    assert( buffer_io != NULL );
    
    return net_buffer_is_valid( buffer_io->buffer );
}

enet_booleans net_buffer_io_can(
    const net_buffer_io_t* buffer_io,
    const enet_buffer_io_modes mode
) {
    if ( net_buffer_io_is_valid( buffer_io ) == enet_false || !( buffer_io->mode & mode ) )
        return enet_false;

    return enet_true;
}

enet_booleans net_buffer_io_is_eof( const net_buffer_io_t* buffer_io ) {
    assert( net_buffer_io_is_valid( buffer_io ) == enet_true );

    if ( buffer_io->head == buffer_io->buffer->size )
        return enet_true;

    return enet_false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// FILE
/////////////////////////////////////////////////////////////////////////////////////////////////
enet_booleans net_file_open(
    net_file_t* file,
    const enet_buffer_io_modes mode,
    const char* path
) {
    assert( file != NULL );
    assert( path != NULL );

    const char* file_mode = "rb";

    if ( mode == enet_buffer_io_write )
        file_mode = "wb";
    else if ( mode == enet_buffer_io_read_write )
        file_mode = "ab+";
    
    file->mode = mode;
    file->file = fopen( path, file_mode );

    if ( net_file_is_valid( file ) == enet_false )
        return enet_false;

    struct stat st;
    memset( &st, 0x00, sizeof( struct stat ) );
    if ( fstat( fileno( file->file ), &st ) == 0 )
        file->size = (uint32_t)st.st_size;
    else
        file->size = 0;

    return enet_true;
}

enet_booleans net_file_read( net_file_t* file, net_buffer_t* buffer ) {
    assert( file != NULL );
    assert( net_buffer_is_valid( buffer ) == enet_true );

    if ( net_file_is_valid( file ) == enet_false )
        return enet_false;

    const size_t readed = fread( buffer->data, sizeof( uint8_t ), buffer->size, file->file );

    if ( readed == buffer->size )
        return enet_true;

    return enet_false;
}

enet_booleans net_file_write( net_file_t* file, net_buffer_t* buffer ) {
    assert( file != NULL );
    assert( net_buffer_is_valid( buffer ) == enet_true );

    if ( net_file_is_valid( file ) == enet_false )
        return enet_false;        

    const size_t writed = fwrite( buffer->data, sizeof( uint8_t ), buffer->size, file->file );

    if ( writed == buffer->size )
        return enet_true;

    return enet_false;
}

void net_file_jump( net_file_t* file, const uint32_t offset ) {
    assert( net_file_is_valid( file ) == enet_true );

    fseek( file->file, offset, SEEK_CUR );
}

enet_booleans net_file_is_valid( const net_file_t* file ) {
    assert( file != NULL );
    
    if ( file->file != NULL )
        return enet_true;

    return enet_false;
}

enet_booleans net_file_is_eof( const net_file_t* file ) {
    assert( net_file_is_valid( file ) == enet_true );

    if ( feof( file->file ) )
        return enet_true;

    return enet_false;
}

void net_file_close( net_file_t* file ) {
    assert( file != NULL );

    if ( net_file_is_valid( file ) == enet_false )
        return;

    fclose( file->file );
    memset( file, 0x00, sizeof( net_file_t ) );
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// INPUTS
/////////////////////////////////////////////////////////////////////////////////////////////////
void net_clear_input( ) {
    int trash = 0;

    do
        trash = getchar( );
    while ( trash != '\n' && trash != EOF );
}

enet_booleans net_read_input( net_buffer_t* buffer, enet_booleans wait_for_inputs ) {
    if ( net_buffer_is_valid( buffer ) == enet_false )
        return enet_false;

    if ( wait_for_inputs == enet_false ) {
        const int flags = fcntl( STDIN_FILENO, F_GETFL, 0 );

        fcntl( STDIN_FILENO, F_SETFL, flags | O_NONBLOCK );

        if ( fgets( buffer->data, buffer->length - 1, stdin ) == NULL ) {
            fcntl( STDIN_FILENO, F_SETFL, flags );
            return enet_false;
        }
    } else
        while ( fgets( buffer->data, buffer->length - 1, stdin ) == NULL );

    buffer->size = strlen( (const char*)buffer->data );

    if ( strchr( buffer->data, '\n' ) == NULL )
        net_clear_input( );

    if ( buffer->size - 1 > 0 ) {
        const uint32_t length = (uint32_t)strlen( (const char*)buffer->data );

        ((char*)buffer->data)[ length - 1 ] = '\0';

        return enet_true;
    }

    return enet_false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// CRYPTO ( RSA-TOY )
/////////////////////////////////////////////////////////////////////////////////////////////////
void net_crypto_init_seed( uint32_t seed ) {
    srand( seed );
}

uint64_t net_crypto_modular_pow(
    uint64_t base,
    const net_crypto_key_t* key
) {
    if ( key == NULL || key->modulus == 1 )
        return 0;

    uint64_t exponent = key->exponent;
    uint64_t result = 1;

    base %= key->modulus;

    while ( exponent > 0 ) {
        if ( exponent & 1 )
            result = (result * base) % key->modulus;

        base = (base * base) % key->modulus;

        exponent >>= 1;
    }

    return result;
}

uint64_t net_crypto_rand( const uint64_t modulus, uint64_t offset ) {
    return ( rand( ) % modulus ) + offset;
}

enet_booleans net_crypto_is_prime( uint64_t num ) {
    if ( num < 2 || num % 2 == 0 || num % 3 == 0 )
        return enet_false;

    if ( num < 4 )
        return enet_true;

    for ( uint64_t i = 5; i * i <= num; i = i + 6 ) {
        if ( num % i == 0 || num % (i + 2) == 0 )
            return enet_false;
    }

    return enet_true;
}

uint64_t net_crypto_next_prime( const uint64_t base ) {
    uint64_t num = ( base <= 1 ) ? 2 : base;

    while ( net_crypto_is_prime( num ) == enet_false )
        num += 1;

    return num;
}

uint64_t net_crypto_gcd( uint64_t a, uint64_t b ) {
    while ( b ) {
        a %= b;

        uint64_t t = a;
        a = b;
        b = t;
    }

    return a;
}

enet_booleans net_crypto_generate_keys(
    net_crypto_key_t* restrict public,
    net_crypto_key_t* restrict private
) {
    uint64_t p = 0;
    uint64_t q = 0;
    uint64_t n = p * q;
    uint64_t phi_n = ( p - 1 ) * ( q - 1 );
    uint64_t e = 7;

    do {
        p = net_crypto_rand( 500, 1000 );
        q = net_crypto_rand( 500, 1500 );
        p = net_crypto_next_prime( p );
        q = net_crypto_next_prime( q );

        if ( p == q )
            q = net_crypto_next_prime( q + 1 );

        n = p * q;
        phi_n = ( p - 1 ) * ( q - 1 );
    } while( net_crypto_gcd( e, phi_n ) != 1 );

    public->exponent = e;
    public->modulus = n;
    private->modulus = n;

    for ( uint64_t k = 1; k < phi_n; k++ ) {
        if ( (k * e) % phi_n == 1 ) {
            private->exponent = k;

            return enet_true;
        }
    }

    return enet_false;
}

size_t net_crypto_get_block_bytes( const uint64_t modulus ) {
    assert( modulus != 0 );

    size_t block_size = 0;
    size_t temp_m = modulus;

    while ( temp_m > 0 ) {
        temp_m >>= 1;
        block_size += 1;
    }

    const size_t byte_count = (block_size - 1) / 8;

    if ( byte_count > 0 )
        return byte_count;

    return 1;
}

uint64_t net_crypto_pack_block( const uint8_t *data, const size_t length ) {
    assert( data != NULL );

    uint64_t packed = 0;

    for ( size_t i = 0; i < length; i++ )
        packed = (packed << 8) | data[i];

    return packed;
}

void net_crypto_unpack_block(
    const uint64_t packed,
    uint8_t* data, 
    const size_t length
) {
    assert( data != NULL );

    uint64_t temp_p = packed;

    for ( size_t i = 0; i < length; i++ ) {
        data[length - i - 1] = temp_p & 0xFF;

        temp_p >>= 8;
    }
}

enet_booleans net_crypto_encrypt(
    const net_crypto_key_t* key,
    const net_buffer_t* restrict src,
    net_buffer_t* restrict dst
) {
    assert(net_crypto_is_key_valid(key) == enet_true);
    assert(net_buffer_is_valid(src) == enet_true);
    assert(src->size > 0);

    const size_t block_bytes = net_crypto_get_block_bytes( key->modulus );
    const size_t block_count = ( src->size + block_bytes - 1 ) / block_bytes;
    const size_t out_size = block_count * sizeof(uint64_t);

    if ( net_buffer_create( dst, out_size ) == enet_false )
        return enet_false;

    net_buffer_resize( dst, out_size );

    const uint8_t* src_buffer = (const uint8_t*)src->data;
    uint64_t* dst_buffer = (uint64_t*)dst->data;

    for ( size_t i = 0; i < block_count; i++ ) {
        const size_t offset = i * block_bytes;
        size_t len = block_bytes;

        if ( offset + len > src->size )
            len = src->size - offset;

        const uint64_t packed = net_crypto_pack_block( src_buffer + offset, len );

        assert( packed < key->modulus );

        dst_buffer[ i ] = net_crypto_modular_pow( packed, key );
    }

    return enet_true;
}

enet_booleans net_crypto_decrypt(
    const net_crypto_key_t* key,
    const net_buffer_t* restrict src,
    net_buffer_t* restrict dst
) {
    assert( net_crypto_is_key_valid( key ) == enet_true );
    assert( net_buffer_is_valid( src ) == enet_true );
    assert( src->size > 0 );

    const size_t block_bytes = net_crypto_get_block_bytes( key->modulus );
    const size_t block_count = src->size / sizeof(uint64_t);
    const size_t out_size = block_bytes * block_count;

    if ( net_buffer_create( dst, out_size ) == enet_false )
        return enet_false;

    net_buffer_resize( dst, out_size );

    const uint64_t* src_buffer = (const uint64_t*)src->data;
    uint8_t* dst_buffer = (uint8_t*)dst->data;

    for ( size_t i = 0; i < block_count; i++ ) {
        const size_t offset = i * block_bytes;
        const uint64_t packed = net_crypto_modular_pow( src_buffer[ i ], key );

        net_crypto_unpack_block(packed, dst_buffer + offset, block_bytes);
    }

    return enet_true;
}

enet_booleans net_crypto_is_key_valid( const net_crypto_key_t* key ) {
    assert( key != NULL );

    if ( key->exponent == 0 || key->modulus == 0 )
        return enet_false;

    return enet_true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// THREADS
/////////////////////////////////////////////////////////////////////////////////////////////////
void net_thread_start( net_thread_t* thread, const net_socket_t* client_socket ) {
    assert( thread != NULL );
    assert( client_socket != NULL );

    net_thread_mutex_lock( thread );

    thread->context.status = enet_thread_init;

    memmove( &thread->context.socket, client_socket, sizeof(net_socket_t) );
    memset( &thread->context.crypto_client, 0x00, sizeof(net_crypto_key_t) );
    memset( &thread->context.crypto_server, 0x00, sizeof(net_crypto_key_t) );

    net_thread_mutex_unlock( thread );
}

void net_thread_mutex_lock( net_thread_t* thread ) {
    assert( thread != NULL );

    pthread_mutex_lock( &thread->mutex );
}

void net_thread_mutex_unlock( net_thread_t* thread ) {
    assert( thread != NULL );

    pthread_mutex_unlock( &thread->mutex );
}

void net_thread_set_status(
    net_thread_t* thread,
    const enet_thread_status status
) {
    net_thread_mutex_lock( thread );

    thread->context.status = status;

    net_thread_mutex_unlock( thread );
}

enet_thread_status net_thread_get_status( net_thread_t* thread ) {
    assert( thread != NULL );

    net_thread_mutex_lock( thread );

    const enet_thread_status status = thread->context.status;

    net_thread_mutex_unlock( thread );

    return status;
}

enet_booleans net_thread_create( net_thread_t* thread, net_thread_functor_t functor ) {
    assert( thread != NULL );

    if ( pthread_mutex_init( &thread->mutex, NULL ) != 0 ) {
        net_print_error( "Can't create mutex for thread %p", thread );

        return enet_false;
    }

    if ( pthread_create( &thread->thread, NULL, functor, thread ) != 0 ) {
        net_print_error( "Can't create thread instance for thread %p", thread );
        pthread_mutex_destroy( &thread->mutex );

        return enet_false;
    }

    net_thread_mutex_lock( thread );

    thread->context.status = enet_thread_pending;

    net_thread_mutex_unlock( thread );

    return enet_true;
}

void net_thread_destroy( net_thread_t* thread ) {
    if ( thread == NULL )
        return;

    pthread_join( thread->thread, NULL );
    pthread_mutex_destroy( &thread->mutex );
}

void net_thread_pool_destroy_range(
    net_thread_t* thread_list,
    const uint32_t thread_count
) {
    uint32_t thread_id = thread_count;

    while ( thread_id-- > 0 ) {
        net_thread_t* thread = thread_list + thread_id;

        net_thread_set_status( thread, enet_thread_alt );
        net_thread_destroy( thread );
    }
}

enet_booleans net_thread_pool_create(
    net_thread_pool_t* thread_pool,
    uint32_t thread_count,
    net_thread_functor_t functor
) {
    assert( thread_pool != NULL );
    assert( thread_count > 0 );
    assert( functor != NULL );

    net_thread_t* thread_list = (net_thread_t*)malloc( thread_count * sizeof(net_thread_t) );

    if ( thread_list == NULL ) {
        net_print_error( "Can't allocate %u thread for thread pool %p", thread_count, thread_pool );

        return enet_false;
    }

    uint32_t created_thread_count = 0;

    while ( created_thread_count < thread_count ) {
        net_thread_t* thread = thread_list + created_thread_count;

        if ( net_thread_create( thread, functor ) == enet_true )
            created_thread_count += 1;
        else
            break;
    }

    if ( created_thread_count < thread_count ) {
        net_thread_pool_destroy_range( thread_list, created_thread_count );

        return enet_false;
    }

    thread_pool->thread_list  = thread_list;
    thread_pool->thread_count = thread_count;

    return enet_true;
}

net_thread_t* net_thread_pool_acquire( net_thread_pool_t* thread_pool ) {
    assert( thread_pool != NULL );

    uint32_t thread_id = 0;

    while ( thread_id < thread_pool->thread_count ) {
        net_thread_t* thread = thread_pool->thread_list + thread_id;

        if ( net_thread_get_status( thread ) == enet_thread_pending )
            return thread;

        thread_id += 1;
    }

    return NULL;
}

enet_booleans net_thread_pool_is_valid( const net_thread_pool_t* thread_pool ) {
    assert( thread_pool != NULL );

    if ( thread_pool->thread_list == NULL || thread_pool->thread_count == 0 )
        return enet_false;

    return enet_true;
}

enet_booleans net_thread_pool_is_empty( const net_thread_pool_t* thread_pool ) {
    assert( net_thread_pool_is_valid( thread_pool ) == enet_true );

    uint32_t thread_id = 0;

    while ( thread_id < thread_pool->thread_count ) {
        net_thread_t* thread = thread_pool->thread_list + thread_id;

        if ( net_thread_get_status( thread ) != enet_thread_pending )
            return enet_false;

        thread_id += 1;
    }

    return enet_true;
}

void net_thread_pool_destroy( net_thread_pool_t* thread_pool ) {
    assert( thread_pool != NULL );

    if ( thread_pool->thread_list == NULL )
        return;

    net_thread_pool_destroy_range( thread_pool->thread_list, thread_pool->thread_count );

    free( thread_pool->thread_list );

    thread_pool->thread_list  = NULL;
    thread_pool->thread_count = 0;
}
