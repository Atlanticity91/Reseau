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

#ifndef _NET_UTILS_H_
#define _NET_UTILS_H_

#define _GNU_SOURCE // Pour inclure les fonctions de endian.h

#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>

#define net_unused(name) ((void)name)

/////////////////////////////////////////////////////////////////////////////////////////////////
// HELPER
/////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum enet_booleans {
    enet_false = 0,
    enet_true
} enet_booleans;

void net_print_error( const char *format, ... );

uint32_t parse_uint32( char* string );

/////////////////////////////////////////////////////////////////////////////////////////////////
// BUFFER
/////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * net_buffer_t struct
 * @field length current buffer allocated length in bytes.
 * @field siwe current buffer size from 0 to length.
 * @field data current buffer memory.
 **/
typedef struct net_buffer_t {
    uint32_t length;
    uint32_t size;
    void* data;
} net_buffer_t;

net_buffer_t net_buffer_immutable( const char* text );

net_buffer_t net_buffer_reference( const net_buffer_t* other, const uint32_t offset );

enet_booleans net_buffer_create( net_buffer_t* buffer, const uint32_t length );

enet_booleans net_buffer_resize( net_buffer_t* buffer, const uint32_t size );

void net_buffer_clear( net_buffer_t *buffer );

enet_booleans net_buffer_is_valid( const net_buffer_t* buffer );

enet_booleans net_buffer_is_full( const net_buffer_t* buffer );

enet_booleans net_buffer_is_empty( const net_buffer_t* buffer );

enet_booleans net_buffer_contain( const net_buffer_t* buffer, const char* string );

uint8_t* net_buffer_get_raw( const net_buffer_t* buffer );

void net_buffer_destroy( net_buffer_t* buffer );

/////////////////////////////////////////////////////////////////////////////////////////////////
// BUFFER IO
/////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum enet_buffer_io_modes {
    enet_buffer_io_read  = 1 << 0,
    enet_buffer_io_write = 1 << 1,
    enet_buffer_io_read_write = enet_buffer_io_read | enet_buffer_io_write
} enet_buffer_io_modes;

typedef struct net_buffer_io_t {
    enet_buffer_io_modes mode;
    net_buffer_t* buffer;
    uint32_t head;
} net_buffer_io_t;

net_buffer_io_t net_buffer_io_acquire(
    net_buffer_t* buffer,
    const enet_buffer_io_modes mode
);

void net_buffer_io_reset( net_buffer_io_t* buffer_io );

enet_booleans net_buffer_io_read_int8( net_buffer_io_t* buffer_io, int8_t* out );
enet_booleans net_buffer_io_read_int16( net_buffer_io_t* buffer_io, int16_t* out );
enet_booleans net_buffer_io_read_int32( net_buffer_io_t* buffer_io, int32_t* out );
enet_booleans net_buffer_io_read_int64( net_buffer_io_t* buffer_io, int64_t* out );

enet_booleans net_buffer_io_read_uint8( net_buffer_io_t* buffer_io, uint8_t* out );
enet_booleans net_buffer_io_read_uint16( net_buffer_io_t* buffer_io, uint16_t* out );
enet_booleans net_buffer_io_read_uint32( net_buffer_io_t* buffer_io, uint32_t* out );
enet_booleans net_buffer_io_read_uint64( net_buffer_io_t* buffer_io, uint64_t* out );

enet_booleans net_buffer_io_read_raw(
    net_buffer_io_t* buffer_io, 
    char* out,
    const size_t out_length,
    size_t *out_remaining
);

enet_booleans net_buffer_io_write_int8( net_buffer_io_t* buffer_io, const int8_t value );
enet_booleans net_buffer_io_write_int16( net_buffer_io_t* buffer_io, const int16_t value );
enet_booleans net_buffer_io_write_int32( net_buffer_io_t* buffer_io, const int32_t value );
enet_booleans net_buffer_io_write_int64( net_buffer_io_t* buffer_io, const int64_t value );

enet_booleans net_buffer_io_write_uint8( net_buffer_io_t* buffer_io, const uint8_t value );
enet_booleans net_buffer_io_write_uint16( net_buffer_io_t* buffer_io, const uint16_t value );
enet_booleans net_buffer_io_write_uint32( net_buffer_io_t* buffer_io, const uint32_t value );
enet_booleans net_buffer_io_write_uint64( net_buffer_io_t* buffer_io, const uint64_t value );

enet_booleans net_buffer_io_write_raw(
    net_buffer_io_t* buffer_io,
    const char* data,
    const size_t data_length,
    size_t* out_remaining
);

enet_booleans net_buffer_io_jump( net_buffer_io_t* buffer_io, const uint32_t length );

enet_booleans net_buffer_io_is_valid( const net_buffer_io_t* buffer_io );

enet_booleans net_buffer_io_can(
    const net_buffer_io_t* buffer_io,
    const enet_buffer_io_modes mode
);

enet_booleans net_buffer_io_is_eof( const net_buffer_io_t* buffer_io );

/////////////////////////////////////////////////////////////////////////////////////////////////
// FILE
/////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct net_file_t {
    enet_buffer_io_modes mode;
    FILE* file;
    uint32_t size;
} net_file_t;

enet_booleans net_file_open(
    net_file_t* file,
    const enet_buffer_io_modes mode,
    const char* path
);

enet_booleans net_file_read( net_file_t* file, net_buffer_t* buffer );

enet_booleans net_file_write( net_file_t* file, net_buffer_t* buffer );

void net_file_jump( net_file_t* file, const uint32_t offset );

enet_booleans net_file_is_valid( const net_file_t* file );

enet_booleans net_file_is_eof( const net_file_t* file );

void net_file_close( net_file_t* file );

/////////////////////////////////////////////////////////////////////////////////////////////////
// INPUTS
/////////////////////////////////////////////////////////////////////////////////////////////////
enet_booleans net_read_input( net_buffer_t *buffer, enet_booleans wait_for_inputs );

/////////////////////////////////////////////////////////////////////////////////////////////////
// CRYPTO ( RSA-TOY )
/////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct net_crypto_key_t {
    uint64_t exponent;
    uint64_t modulus;
} net_crypto_key_t;

void net_crypto_init_seed( uint32_t seed );

enet_booleans net_crypto_generate_keys(
    net_crypto_key_t* restrict public,
    net_crypto_key_t* restrict private
);

enet_booleans net_crypto_encrypt(
    const net_crypto_key_t* key,
    const struct net_buffer_t* restrict src,
    struct net_buffer_t* restrict dst
);

enet_booleans net_crypto_decrypt(
    const net_crypto_key_t* key,
    const struct net_buffer_t* restrict src,
    struct net_buffer_t* restrict dst
);

enet_booleans net_crypto_is_key_valid( const net_crypto_key_t* key );

/////////////////////////////////////////////////////////////////////////////////////////////////
// SOCKET
/////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum enet_socket_types {
    enet_socket_server = 0,
    enet_socket_client
} enet_socket_types;

typedef enum enet_socket_protocoles {
    enet_socket_tcp = 0,
    enet_socket_udp
} enet_socket_protocoles;

typedef struct net_socket_t {
    enet_socket_types type;
    enet_socket_protocoles protocole;
    int32_t descriptor;
    struct sockaddr_in address;
} net_socket_t;

void net_socket_init( net_socket_t* socket );

enet_booleans net_socket_create_server(
    net_socket_t* socket,
    const int32_t port,
    const uint32_t max_client_count,
    const enet_socket_protocoles protocole
);

enet_booleans net_socket_create_client(
    net_socket_t* socket,
    const char* address,
    const int32_t port,
    const enet_socket_protocoles protocole
);

enet_booleans net_socket_accept( const net_socket_t* server, net_socket_t* client );

enet_booleans net_socket_send( net_socket_t* socket, net_buffer_t* buffer );

enet_booleans net_socket_recv( net_socket_t* socket, net_buffer_t* buffer );

enet_booleans net_socket_is_valid( const net_socket_t* socket );

enet_booleans net_socket_is_type( const net_socket_t* socket, const enet_socket_types type );

enet_booleans net_socket_is(
    const net_socket_t* socket, 
    const enet_socket_protocoles protocole
);

void net_socket_destroy( net_socket_t* socket );

/////////////////////////////////////////////////////////////////////////////////////////////////
// THREADS
/////////////////////////////////////////////////////////////////////////////////////////////////
typedef void *(*net_thread_functor_t)(void *);

typedef enum enet_thread_status {
    enet_thread_pending = 0,
    enet_thread_init,
    enet_thread_running,
    enet_thread_alt
} enet_thread_status;

typedef struct net_thread_context_t {
    enet_thread_status status;
    net_socket_t socket;
    net_crypto_key_t crypto_server;
    net_crypto_key_t crypto_client;
} net_thread_context_t;

typedef struct net_thread_t {
    pthread_t thread;
    pthread_mutex_t mutex;
    net_thread_context_t context;
} net_thread_t;

typedef struct net_thread_pool_t {
    net_thread_t *thread_list;
    uint32_t thread_count;
} net_thread_pool_t;

enet_booleans net_thread_create( net_thread_t* thread, net_thread_functor_t functor ) ;

void net_thread_destroy( net_thread_t* thread );

void net_thread_start( net_thread_t* thread, const net_socket_t* client_socket );

void net_thread_mutex_lock( net_thread_t* thread );

void net_thread_mutex_unlock( net_thread_t* thread );

void net_thread_set_status(
    net_thread_t* thread,
    const enet_thread_status status
);

enet_thread_status net_thread_get_status( net_thread_t* thread );

enet_booleans net_thread_pool_create(
    net_thread_pool_t* thread_pool,
    uint32_t thread_count,
    net_thread_functor_t functor
);

net_thread_t *net_thread_pool_acquire( net_thread_pool_t* thread_pool );

enet_booleans net_thread_pool_is_valid( const net_thread_pool_t* thread_pool );

enet_booleans net_thread_pool_is_empty( const net_thread_pool_t* thread_pool );

void net_thread_pool_destroy( net_thread_pool_t* thread_pool );

#endif /* !_NET_UTILS_H_ */
