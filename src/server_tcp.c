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
#include "net_global.h"

#define DB_FILE "db.bin"

typedef struct server_db_entry_t {
    char* name;
    char* path;
} server_db_entry_t;

typedef struct server_context_t { 
    pthread_mutex_t mutex;
    server_db_entry_t* db;
    uint32_t count;
} server_context_t;

server_context_t* context = NULL;

enet_booleans load_db( ) {
    context = (server_context_t*)malloc( sizeof( server_context_t ) );
    memset( context, 0x00, sizeof( server_context_t ) );

    if ( pthread_mutex_init( &context->mutex, NULL ) != 0 )
        return enet_false;

    FILE* file = fopen( DB_FILE, "rb" );

    if ( file == NULL )
        return enet_true;

    pthread_mutex_lock( &context->mutex );
    
    if ( fread( &context->count, sizeof( uint32_t ), 1, file ) == 0 ) {
        pthread_mutex_unlock( &context->mutex );
        fclose( file );
        return enet_false;
    }

    context->db = (server_db_entry_t*)malloc( sizeof( server_db_entry_t ) * context->count );

    uint32_t id = 0;
    while ( id < context->count ) {
        server_db_entry_t* entry = &context->db[ id ];

        uint32_t length = 0;
        fread( &length, sizeof( uint32_t ), 1, file );
        entry->name = (char*)malloc( sizeof( char ) * length + 1 );
        fread( entry->name, sizeof( char ), length, file );
        entry->name[ length ] = '\0';

        fread( &length, sizeof( uint32_t ), 1, file );
        entry->path = (char*)malloc( sizeof( char ) * length + 1 );
        fread( entry->path, sizeof( char ), length, file );
        entry->path[ length ] = '\0';

        id += 1;
    }
    
    pthread_mutex_unlock( &context->mutex );

    fclose( file );

    return enet_true;
}

char* acquire_user( const char* user ) {
    assert( user != NULL );

    const uint32_t length = (uint32_t)strlen( user );
    uint32_t id = 0;

    pthread_mutex_lock( &context->mutex );
    while ( id < context->count ) {
        server_db_entry_t* entry = &context->db[ id ];

        if ( memmem( entry->name, strlen( entry->name ), user, length ) != NULL ) {
            pthread_mutex_unlock( &context->mutex );
            return entry->path;
        }

        id += 1;
    }
    pthread_mutex_unlock( &context->mutex );

    return NULL;
}

char* emplace_user( const char* user ) {
    assert( user != NULL );

    pthread_mutex_lock( &context->mutex );

    context->count += 1;

    if ( context->db != NULL )
        context->db = (server_db_entry_t*)realloc( context->db, sizeof( server_db_entry_t ) * context->count );
    else 
        context->db = (server_db_entry_t*)malloc( sizeof( server_db_entry_t ) * context->count );

    assert( context->db != NULL );

    uint64_t uuid = 0;
    for ( int i = 0; i < 8; i++ )
        uuid = ( uuid << 8 ) | ( rand( ) & 0xFF );

    server_db_entry_t* entry = &context->db[ context->count - 1 ];
    const uint32_t length = (uint32_t)strlen( user );

    entry->name = (char*)malloc( sizeof( char ) * length + 1);
    entry->name[ length ] = '\0';
    entry->path = (char*)malloc( sizeof( char ) * 33 );

    memmove( entry->name, user, length );
    snprintf( entry->path, 33, "%" PRIu64, uuid );

    pthread_mutex_unlock( &context->mutex );

    return entry->path;
}

void save_db( ) {
    FILE* file = fopen( DB_FILE, "wb" );

    if ( file == NULL )
        return;

    fwrite( &context->count, sizeof( uint32_t ), 1, file );

    uint32_t id = 0;
    while ( id < context->count ) {
        server_db_entry_t* entry = &context->db[ id ];

        const uint32_t name_length = (uint32_t)strlen( entry->name );
        const uint32_t path_length = (uint32_t)strlen( entry->path );

        fwrite( &name_length, sizeof( uint32_t ), 1, file );
        fwrite( entry->name, sizeof( char ), name_length, file );
        fwrite( &path_length, sizeof( uint32_t ), 1, file );
        fwrite( entry->path, sizeof( char ), path_length, file );

        id += 1;
    }

    fclose( file );

    printf( "> DB saved as %s.\n", DB_FILE );
}

void destroy_context( ) {
    save_db( );

    pthread_mutex_destroy( &context->mutex );

    uint32_t id = 0;
    while ( id < context->count ) {
        server_db_entry_t* entry = &context->db[ id ];

        free( entry->name );
        free( entry->path );

        id += 1;
    }

    free( context );
}

void parse_arguments(
    int argc,
    char** argv,
    uint32_t* port,
    uint32_t* thread_count,
    uint32_t* crypto_seed
) {
    for ( int i = 0; i < argc; i++ ) {
        if ( argv[ i ][ 0 ] != '-' )
            continue;

        switch ( tolower( argv[ i ][ 1 ] ) ) {
            case 'p' : (*port) = parse_uint32( argv[ i ] + 2 ); break;
            case 'c' : (*thread_count) = parse_uint32( argv[ i ] + 2 ); break;
            case 's' : (*crypto_seed) = parse_uint32( argv[ i ] + 2 ); break;

            default : break;
        }
    }
}

void thread_init_client_exit(
    net_buffer_t* buffer,
    net_thread_context_t* thread_context,
    net_thread_t* thread
) {
    net_buffer_destroy( buffer );
    net_socket_destroy( &thread_context->socket );
    net_thread_set_status( thread, enet_thread_pending );
}

void thread_init_client(
    net_thread_t* thread,
    net_thread_context_t* thread_context
) {
    assert( thread != NULL );
    assert( thread_context != NULL );

    net_buffer_t buffer;
    memset( &buffer, 0x00, sizeof( net_buffer_t ) );
    
    if ( net_buffer_create( &buffer, 2 * sizeof( uint64_t ) ) == enet_false ) {
        net_socket_destroy( &thread_context->socket );
        net_thread_set_status( thread, enet_thread_pending );
        return;
    }

    if ( net_socket_recv( &thread_context->socket, &buffer ) == enet_false ) {
        thread_init_client_exit( &buffer, thread_context, thread );
        return;
    }

    net_buffer_io_t buffer_io = net_buffer_io_acquire( &buffer, enet_buffer_io_read_write );

    net_thread_mutex_lock( thread );
    if ( 
        net_buffer_io_read_uint64( &buffer_io, &thread_context->crypto_client.exponent ) == enet_false ||
        net_buffer_io_read_uint64( &buffer_io, &thread_context->crypto_client.modulus ) == enet_false
    ) {
        net_thread_mutex_unlock( thread );
        thread_init_client_exit( &buffer, thread_context, thread );
        return;
    }
    net_thread_mutex_unlock( thread );

    net_buffer_io_reset( &buffer_io );

    net_crypto_key_t server_public;

    net_thread_mutex_lock( thread );
    if ( net_crypto_generate_keys( &server_public, &thread_context->crypto_server ) == enet_false ) {
        net_thread_mutex_unlock( thread );
        thread_init_client_exit( &buffer, thread_context, thread );
        return;
    }
    net_thread_mutex_unlock( thread );

    if ( 
        net_buffer_io_write_uint64( &buffer_io, server_public.exponent ) == enet_false ||
        net_buffer_io_write_uint64( &buffer_io, server_public.modulus ) == enet_false
    ) {
        thread_init_client_exit( &buffer, thread_context, thread );
        return;
    }
    
    if ( net_socket_send( &thread_context->socket, &buffer ) == enet_false ) {
        thread_init_client_exit( &buffer, thread_context, thread );
        return;
    }
    
    printf( 
        "New Client [\n\tServer Private : { %lu - %lu }\n\tClient Public : { %lu - %lu }\n]\n", 
        thread_context->crypto_server.exponent, thread_context->crypto_server.modulus, 
        thread_context->crypto_client.exponent, thread_context->crypto_client.modulus
    );

    net_buffer_destroy( &buffer );
    net_thread_set_status( thread, enet_thread_running );
}

enet_booleans net_send( 
    net_thread_context_t* thread_context,
    const net_buffer_t* buffer
) {
    net_buffer_t cypher_buffer;
    memset( &cypher_buffer, 0x00, sizeof( cypher_buffer ) );

    net_crypto_encrypt( &thread_context->crypto_server, buffer, &cypher_buffer );

    enet_booleans result = net_socket_send( &thread_context->socket, &cypher_buffer );

    net_buffer_destroy( &cypher_buffer );

    return result;
}

enet_booleans net_send_status(
    net_thread_context_t* thread_context,
    const enet_command_t command
) {
    net_buffer_t buffer;
    memset( &buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &buffer, sizeof( uint32_t ) ) == enet_false )
        return enet_false;

    net_buffer_io_t buffer_io = net_buffer_io_acquire( &buffer, enet_buffer_io_write );

    net_buffer_io_write_uint32( &buffer_io, command );

    enet_booleans result = net_send( thread_context, &buffer );

    net_buffer_destroy( &buffer );

    return result;
}

void server_quit( net_thread_t* thread, net_thread_context_t* thread_context ) {
    printf( "> Client %p : quit\n", &thread_context->socket );
    
    net_thread_mutex_lock( thread );
    net_socket_destroy( &thread_context->socket );
    net_thread_mutex_unlock( thread );
    net_thread_set_status( thread, enet_thread_pending );
}

void server_lost_client( net_thread_t* thread, net_thread_context_t* thread_context ) {
    printf( "> Client %p lost.\n", &thread_context->socket );
    net_thread_set_status( thread, enet_thread_pending );
}

void server_send(
    net_thread_t* thread,
    net_thread_context_t* thread_context,
    net_buffer_io_t* client_input,
    char* path
) {
    printf( "> Client %p : send\n", &thread_context->socket );

    if ( path == NULL ) {
        if ( net_send_status( thread_context, enet_command_bad_name ) == enet_false ) {
            server_lost_client( thread, thread_context );
            return;
        }
    }

    net_file_t file;
    memset( &file, 0x00, sizeof( net_file_t ) );

    if ( net_file_open( &file, enet_buffer_io_read_write, (const char*)path ) == enet_false ) {
        printf( "> Can't store local copy of receive file.\n" );
        
        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            printf( "> Client %p lost.\n", &thread_context->socket );

        return;
    }

    if ( file.size > 0 )
        net_file_jump( &file, file.size );

    uint32_t name_length = 0;
    net_buffer_io_read_uint32( client_input, &name_length );

    uint32_t content_length = 0;
    net_buffer_io_read_uint32( client_input, &content_length );

    net_buffer_t name = net_buffer_reference( client_input->buffer, 3 * sizeof( uint32_t ) );
    net_buffer_t content = net_buffer_reference( client_input->buffer, 3 * sizeof( uint32_t ) + name_length );

    net_buffer_resize( &name, name_length );
    net_buffer_resize( &content, content_length );
    
    fwrite( &name_length, sizeof( uint32_t ), 1, file.file );
    net_file_write( &file, &name );

    fwrite( &content_length, sizeof( uint32_t ), 1, file.file );
    net_file_write( &file, &content );
    net_file_close( &file );

    printf( "> File %s writing completed.\n", (const char*)name.data );

    if ( net_send_status( thread_context, enet_command_ok ) == enet_false )
        server_lost_client( thread, thread_context );
}

void server_list(
    net_thread_t* thread,
    net_thread_context_t* thread_context,
    char* path
) {
    printf( "> Client %p : list\n", &thread_context->socket );

    if ( path == NULL ) {
        if ( net_send_status( thread_context, enet_command_bad_name ) == enet_false ) {
            server_lost_client( thread, thread_context );
            return;
        }
    }

    net_file_t file;
    memset( &file, 0x00, sizeof( net_file_t ) );

    if ( net_file_open( &file, enet_buffer_io_read, path ) == enet_false ) {
        printf( "> Can't open client file.\n" );

        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            server_lost_client( thread, thread_context );
        return;
    }

    if ( file.size == 0 ) {
        net_file_close( &file );

        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            server_lost_client( thread, thread_context );
        return;
    }

    net_buffer_t decypher_buffer;
    memset( &decypher_buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &decypher_buffer, 128 * sizeof( uint32_t ) ) == enet_false ) {
        printf( "> Can't create entry list buffer.\n" );

        net_file_close( &file );

        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            server_lost_client( thread, thread_context );
        return;
    }

    net_buffer_io_t buffer_io = net_buffer_io_acquire( &decypher_buffer, enet_buffer_io_write );

    uint32_t count = 0;
    uint32_t total_length = 2 * (uint32_t)sizeof( uint32_t );

    net_buffer_resize( &decypher_buffer, total_length );
    net_buffer_io_jump( &buffer_io, total_length );

    uint32_t length = 0;
    while ( fread( &length, sizeof( uint32_t ), 1, file.file ) ) {
        if ( decypher_buffer.length < decypher_buffer.size + length + 2*(uint32_t)sizeof( uint32_t) ) {
            if ( net_buffer_create( buffer_io.buffer, 2 * decypher_buffer.length ) == enet_false ) {
                printf( "> Can't create entry list buffer.\n" );

                net_file_close( &file );

                if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
                    server_lost_client( thread, thread_context );
                return;
            }
        }

        net_buffer_io_write_uint32( &buffer_io, length );
        net_buffer_resize( &decypher_buffer, decypher_buffer.size + length );
        net_buffer_io_jump( &buffer_io, length );

        total_length += (uint32_t)sizeof( uint32_t );
        char* in = (char*)net_buffer_get_raw( &decypher_buffer ) + total_length;

        fread( in, sizeof( char ), length, file.file );
        in[ length ] = '\0';

        total_length += length;

        fread( &length, sizeof( uint32_t ), 1, file.file );
        net_file_jump( &file, length );

        count += 1;
    }

    net_buffer_io_reset( &buffer_io );
    net_buffer_io_write_uint32( &buffer_io, (uint32_t)enet_command_ok );
    net_buffer_io_write_uint32( &buffer_io, count );
    net_buffer_resize( &decypher_buffer, total_length );
    net_file_close( &file );

    if ( net_send( thread_context, &decypher_buffer ) == enet_false )
        server_lost_client( thread, thread_context );

    net_buffer_destroy( &decypher_buffer );
}

void server_pull(
    net_thread_t* thread,
    net_thread_context_t* thread_context,
    net_buffer_io_t* client_input,
    char* path
) {
    const char* name = (const char*)client_input->buffer->data + 2 * sizeof( uint32_t );
    printf( "> Client %p : pull %s\n", &thread_context->socket, name );

    if ( path == NULL ) {
        if ( net_send_status( thread_context, enet_command_bad_name ) == enet_false ) {
            server_lost_client( thread, thread_context );
            return;
        }
    }

    net_file_t file;
    memset( &file, 0x00, sizeof( net_file_t ) );

    if ( net_file_open( &file, enet_buffer_io_read, path ) == enet_false ) {
        printf( "> Can't open client file.\n" );

        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            server_lost_client( thread, thread_context );
        return;
    }

    if ( file.size == 0 ) {
        net_file_close( &file );

        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            server_lost_client( thread, thread_context );
        return;
    }

    net_buffer_t decypher_buffer;
    memset( &decypher_buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &decypher_buffer, 128 * sizeof( uint32_t ) ) == enet_false ) {
        printf( "> Can't create entry list buffer.\n" );

        net_file_close( &file );

        if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
            server_lost_client( thread, thread_context );
        return;
    }

    net_buffer_io_t buffer_io = net_buffer_io_acquire( &decypher_buffer, enet_buffer_io_write );

    uint32_t length = 0;
    while ( fread( &length, sizeof( uint32_t ), 1, file.file ) ) {
        if ( decypher_buffer.size + length > decypher_buffer.length ) {
            if ( net_buffer_create( &decypher_buffer, 2 * decypher_buffer.length ) == enet_false ) {
                printf( "> Can't create entry name buffer.\n" );

                net_file_close( &file );

                if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
                    server_lost_client( thread, thread_context );
                return;
            }
        }
        
        net_buffer_resize( &decypher_buffer, length );
        net_file_read( &file, &decypher_buffer );

        fread( &length, sizeof( uint32_t ), 1, file.file );

        if ( net_buffer_contain( &decypher_buffer, name ) == enet_true ) {
            const uint32_t total_length = 2 * (uint32_t)sizeof( uint32_t ) + length;

            if ( net_buffer_create( &decypher_buffer, total_length ) == enet_false ) {
                printf( "> Can't create entry buffer.\n" );

                net_file_close( &file );

                if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
                    server_lost_client( thread, thread_context );
                return;
            }

            char* out = (char*)net_buffer_get_raw( &decypher_buffer ) + 2 * (uint32_t)sizeof( uint32_t );

            fread( out, sizeof( uint8_t ), length, file.file );

            net_buffer_io_reset( &buffer_io );
            net_buffer_io_write_uint32( &buffer_io, (uint32_t)enet_command_ok );
            net_buffer_io_write_uint32( &buffer_io, length );
            net_buffer_resize( &decypher_buffer, total_length );
            net_file_close( &file );

            if ( net_send( thread_context, &decypher_buffer ) == enet_false )
                server_lost_client( thread, thread_context );
            
            net_buffer_destroy( &decypher_buffer );

            return;
        } else
            net_file_jump( &file, length );
    }

    net_file_close( &file );

    net_buffer_destroy( &decypher_buffer );

    if ( net_send_status( thread_context, enet_command_bad ) == enet_false )
        server_lost_client( thread, thread_context );
}

void server_name(
    net_thread_t* thread,
    net_thread_context_t* thread_context,
    net_buffer_io_t* client_input,
    char** path
) {
    const char* name = (const char*)client_input->buffer->data + 2 * sizeof( uint32_t );
    printf( "> Client %p : name %s\n", &thread_context->socket, name );

    uint32_t length = 0;
    net_buffer_io_read_uint32( client_input, &length );

    if ( length == 0 ) {
        if ( net_send_status( thread_context, enet_command_bad_name ) == enet_false ) {
            server_lost_client( thread, thread_context );
            return;
        }
    }
    
    (*path) = acquire_user( name );

    if ( (*path) == NULL ) {
        (*path) = emplace_user( name );

        printf( "> New file %s for client %p\n", *path, &thread_context->socket );
    } else 
        printf( "> File %s for client %p\n", *path, &thread_context->socket );

    save_db( );

    if ( net_send_status( thread_context, enet_command_ok ) == enet_false ) {
        server_lost_client( thread, thread_context );
        return;
    }
}

void thread_run_client(
    net_thread_t* thread,
    net_thread_context_t* thread_context,
    net_buffer_t* cypher_buffer,
    net_buffer_t* decypher_buffer,
    char** path
) {
    assert( thread != NULL );
    assert( thread_context != NULL );

    if ( net_socket_recv( &thread_context->socket, cypher_buffer ) == enet_false ) {
        net_socket_destroy( &thread_context->socket );
        net_thread_set_status( thread, enet_thread_pending );
        return;
    }

    net_crypto_decrypt( &thread_context->crypto_client, cypher_buffer, decypher_buffer );

    net_buffer_io_t buffer_read = net_buffer_io_acquire( decypher_buffer, enet_buffer_io_read );

    uint32_t cmd = 0;
    net_buffer_io_read_uint32( &buffer_read, &cmd );

    switch ( cmd ) {
        case enet_command_quit : server_quit( thread, thread_context ); break;
        case enet_command_send : server_send( thread, thread_context, &buffer_read, *path ); break;
        case enet_command_list : server_list( thread, thread_context, *path ); break;
        case enet_command_pull : server_pull( thread, thread_context, &buffer_read, *path ); break;
        case enet_command_name : server_name( thread, thread_context, &buffer_read, path ); break;

        default : break;
    }
}

void* thread_loop( void* argument ) {
    const uint32_t buffer_length = (uint32_t)16 * sizeof( uint32_t );
    net_thread_t* thread = (net_thread_t*)argument;

    net_buffer_t cypher_buffer;
    memset( &cypher_buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &cypher_buffer, buffer_length ) == enet_false ) {
        printf( "Fail to create default cypher buffer(%ub) for thread %p\n", buffer_length, thread );
        return NULL;
    }

    net_buffer_t decypher_buffer;
    memset( &decypher_buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &decypher_buffer, buffer_length ) == enet_false ) {
        printf( "Fail to create default cypher buffer(%ub) for thread %p\n", buffer_length, thread );
        net_buffer_destroy( &cypher_buffer );
        return NULL;
    }
    
    char* path = NULL;

    while ( enet_true ) {
        enet_thread_status status = net_thread_get_status( thread );

        if ( status == enet_thread_alt )
            break;

        if ( status == enet_thread_init ) {
            path = NULL;
            
            thread_init_client( thread, &thread->context );
        } else if ( status == enet_thread_running )
            thread_run_client( thread, &thread->context, &cypher_buffer, &decypher_buffer, &path );
    }

    net_buffer_destroy( &cypher_buffer );
    net_buffer_destroy( &decypher_buffer );

    return NULL;
}

void print_help( ) {
    printf( "> commands :\n" );
    printf( "> quit : to close the server, only available when no client is connected.\n");
}

int main( int argc, char **argv ) {
    net_socket_t socket;
    net_socket_init( &socket );

    net_socket_t client;
    net_socket_init( &client );

    net_thread_pool_t thread_pool;
    memset( &thread_pool, 0x00, sizeof( net_thread_pool_t ) );

    uint32_t port = LOCAL_PORT;
    uint32_t thread_count = TCP_MAX_CLIENT_COUNT;
    uint32_t crypto_seed = (uint32_t)time( NULL );

    parse_arguments( argc, argv, &port, &thread_count, &crypto_seed );

    net_crypto_init_seed( crypto_seed );
    
    if ( load_db( ) == enet_false ) {
        printf( "> Can't load database.\n" );
        return -1;
    }

    if ( net_socket_create_server( &socket, port, thread_count, enet_socket_tcp ) == enet_false )
        return -1;

    if ( net_thread_pool_create( &thread_pool, thread_count, thread_loop ) == enet_false ) {
        net_socket_destroy( &socket );
        
        return -1;
    }

    net_buffer_t input_buffer;
    memset( &input_buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &input_buffer, 16*sizeof( uint32_t) ) == enet_false ) {
        printf( "s> Can't create input buffer.\n" );
        
        net_socket_destroy( &socket );
        net_thread_pool_destroy( &thread_pool );
        
        return -1;
    }

    printf( "> Server ready with %u user stored\n", context->count );
    
    print_help( );

    while ( enet_true ) {
        if ( net_thread_pool_is_empty( &thread_pool ) == enet_true ) {
            printf( "s> " );
            net_read_input( &input_buffer, enet_true );

            if ( net_buffer_contain( &input_buffer, "quit" ) == enet_true )
                break;
            else if ( net_buffer_contain( &input_buffer, "help" ) == enet_true )
                print_help( );
        }

        if ( net_socket_accept( &socket, &client ) == enet_false )
            continue;

        printf( "New client connected on port %u\n", client.address.sin_port );

        net_thread_t* thread = net_thread_pool_acquire( &thread_pool );

        if ( thread != NULL )
            net_thread_start( thread, &client );
        else {
            net_buffer_t response = net_buffer_immutable( "Connection refused by the server." );
            net_buffer_t buffer;
            memset( &buffer, 0x00, sizeof( net_buffer_t ) );

            net_socket_recv( &client, &buffer );
            net_socket_send( &client, &response );
            net_buffer_destroy( &buffer );
        }

        net_socket_init( &client );
    }

    net_thread_pool_destroy( &thread_pool );

    printf( "> Server closed with %u user stored\n", context->count );

    save_db( );

    return 0;
}
