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

void parse_arguments(
    int argc,
    char** argv,
    char** address,
    uint32_t* port,
    uint32_t* crypto_seed
) {
    for ( int i = 0; i < argc; i++ ) {
        if ( argv[ i ][ 0 ] != '-' )
            continue;

        switch ( tolower( argv[ i ][ 1 ] ) ) {
            case 'p' : (*port) = parse_uint32( argv[ i ] + 2 ); break;
            case 'a' : (*address) = argv[ i ] + 2; break;
            case 's' : (*crypto_seed) = parse_uint32( argv[ i ] + 2 ); break;

            default : break;
        }
    }
}

typedef struct client_context_t {
    net_socket_t socket;
    net_crypto_key_t server_public;
    net_crypto_key_t client_private;
    net_buffer_t cypher_buffer;
    net_buffer_t decypher_buffer;
    net_buffer_io_t buffer_write;
    net_buffer_io_t buffer_read;
} client_context_t;

enet_booleans connect_to(
    client_context_t* context,
    const char* address,
    uint32_t port
) {
    if ( net_socket_create_client( &context->socket, address, port, enet_socket_tcp ) == enet_false )
        return enet_false;

    net_buffer_t buffer;
    memset( &buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &buffer, 2 * sizeof(uint64_t) ) == enet_false ) {
        net_socket_destroy( &context->socket );
        return enet_false;
    }

    net_crypto_key_t client_public;

    if ( net_crypto_generate_keys( &client_public, &context->client_private ) == enet_false ) {
        net_buffer_destroy( &buffer );
        net_socket_destroy( &context->socket );
        return enet_false;
    }

    net_buffer_io_t buffer_io = net_buffer_io_acquire( &buffer, enet_buffer_io_read_write );

    if (
        net_buffer_io_write_uint64( &buffer_io, client_public.exponent ) == enet_false ||
        net_buffer_io_write_uint64( &buffer_io, client_public.modulus ) == enet_false ||
        net_socket_send( &context->socket, &buffer ) == enet_false
    ) {
        net_buffer_destroy( &buffer );
        net_socket_destroy( &context->socket );
        return enet_false;
    }

    net_buffer_io_reset( &buffer_io );

    if ( 
        net_socket_recv( &context->socket, &buffer ) == enet_false ||
        net_buffer_io_read_uint64( &buffer_io, &context->server_public.exponent ) == enet_false ||
        net_buffer_io_read_uint64( &buffer_io, &context->server_public.modulus ) == enet_false 
    ) {
        net_buffer_destroy( &buffer );
        net_socket_destroy( &context->socket );
        return enet_false;
    }
    
    return enet_true;
}

enet_command_t parse_command( const net_buffer_t* input_buffer ) {
    if ( net_buffer_contain( input_buffer, "send" ) == enet_true )
        return enet_command_send;
    else if ( net_buffer_contain( input_buffer, "list" ) == enet_true )
        return enet_command_list;
    else if ( net_buffer_contain( input_buffer, "pull" ) == enet_true )
        return enet_command_pull;
    else if ( net_buffer_contain( input_buffer, "name" ) == enet_true )
        return enet_command_name;

    return enet_command_quit;
}

enet_booleans net_send( client_context_t* context ) {
    assert( context != NULL );

    if ( net_crypto_encrypt( &context->client_private, &context->decypher_buffer, &context->cypher_buffer ) == enet_false )
        return enet_false;

    return net_socket_send( &context->socket, &context->cypher_buffer );
}

enet_booleans net_recv( client_context_t* context ) {
    assert( context != NULL );

    if ( net_socket_recv( &context->socket, &context->decypher_buffer ) == enet_false )
        return enet_false;
    
    net_crypto_decrypt( &context->server_public, &context->decypher_buffer, &context->cypher_buffer );

    return enet_true;
}

enet_booleans client_send( client_context_t* context, net_buffer_t* input_buffer ) {
    const uint32_t cmd_length = 5;
    const uint32_t length = (uint32_t)strlen( (const char*)input_buffer->data );

    if ( length <= cmd_length ) {
        printf( "> You can't send file without givin its path.\n" );
        return enet_true;
    }

    net_file_t file;
    memset( &file, 0x00, sizeof( net_file_t ) );

    const char* path = (const char*)input_buffer->data + cmd_length;

    struct stat st;
    if ( stat( path, &st ) != 0 || !S_ISREG( st.st_mode ) ) {
        printf( "> You can't send a directory.\n" );
        return enet_true;
    }

    if ( net_file_open( &file, enet_buffer_io_read, path ) == enet_false ) {
        printf( "> File %s can't be sent.\n", path );
        return enet_true;
    }

    const uint32_t name_length = ( length - cmd_length ) + 1;
    const uint32_t total_length = 3 * (uint32_t)sizeof( uint32_t ) + name_length + file.size;
    
    if ( net_buffer_create( &context->decypher_buffer, total_length ) == enet_false ) {
        printf( "> Can't create buffer to send data.\n" );
        return enet_false;
    }
        
    net_buffer_io_reset( &context->buffer_write );
    net_buffer_io_write_uint32( &context->buffer_write, enet_command_send );
    net_buffer_io_write_uint32( &context->buffer_write, name_length );
    net_buffer_io_write_uint32( &context->buffer_write, file.size );
    net_buffer_io_write_raw( &context->buffer_write, path, name_length, NULL );

    net_buffer_t ref = net_buffer_reference( &context->decypher_buffer, 3 * (uint32_t)sizeof( uint32_t ) + name_length );
    net_buffer_resize( &ref, file.size );
    net_file_read( &file, &ref );
    net_file_close( &file );
    net_buffer_resize( context->buffer_write.buffer, total_length );

    if (
        net_send( context ) == enet_false ||
        net_recv( context ) == enet_false
    ) {
        printf( "> Connection lost.\n" );
        return enet_false;
    }

    uint32_t status;

    net_buffer_io_read_uint32( &context->buffer_read, &status );

    if ( status == enet_command_ok ) {
        printf( "> Sending of %s succeded.\n", path );
        return enet_true;
    } else if ( status == enet_command_bad_name ) {
        printf( "> You must set your name with \"name\" command before using send.\n" );
        return enet_true;
    } else if ( status == enet_command_bad ) {
        printf( "> Server can't store %s.\n", path );
        return enet_true;
    }
    
    printf( "> Sending failed.\n" );

    return enet_false;
}

enet_booleans client_list( client_context_t* context ) {
    if (
        net_send( context ) == enet_false ||
        net_recv( context ) == enet_false
    ) {
        printf( "> Connection lost.\n" );
        return enet_false;
    }

    uint32_t status;
    net_buffer_io_read_uint32( &context->buffer_read, &status );

    if ( status == enet_command_bad ) {
        printf( "> No entries in the file.\n" );
        return enet_true;
    } else if ( status == enet_command_bad_name ) {
        printf( "> You must set your name with \"name\" command before using list.\n" );
        return enet_true;
    } else if ( status != enet_command_ok ) {
        printf( "> Unknow error.\n" );
        return enet_true;
    }

    uint32_t count = 0;
    net_buffer_io_read_uint32( &context->buffer_read, &count );

    char* out = (char*)net_buffer_get_raw( context->buffer_read.buffer );

    while ( count-- > 0 ) {
        uint32_t length = 0;
        net_buffer_io_read_uint32( &context->buffer_read, &length );
    
        if ( net_buffer_io_is_eof( &context->buffer_read ) == enet_true ) {
            printf( "> Error during listing.\n" );
            return enet_false;
        }
        
        printf( "> Entry : %s\n", (const char*)out + context->buffer_read.head );

        net_buffer_io_jump( &context->buffer_read, length );
    }

    return enet_true;
}

enet_booleans client_pull( client_context_t* context, net_buffer_t* input_buffer ) {
    const uint32_t cmd_length = 5;
    const uint32_t length = (uint32_t)strlen( (const char*)input_buffer->data );

    if ( length <= cmd_length ) {
        printf( "> You can't pull file without givin its entry name.\n" );
        return enet_true;
    }

    if ( net_buffer_create( &context->decypher_buffer, 2 * sizeof( uint32_t ) + length ) == enet_false )
        return enet_false;

    const uint32_t name_length = ( length - cmd_length ) + 1;
    const char* name = (const char*)input_buffer->data + cmd_length;

    net_buffer_io_reset( &context->buffer_write );
    net_buffer_io_write_uint32( &context->buffer_write, enet_command_pull );
    net_buffer_io_write_uint32( &context->buffer_write, name_length );
    net_buffer_io_write_raw( &context->buffer_write, name, name_length, NULL );

    if (
        net_send( context ) == enet_false ||
        net_recv( context ) == enet_false
    ) {
        printf( "> Connection lost.\n" );
        return enet_false;
    }

    uint32_t status;
    net_buffer_io_read_uint32( &context->buffer_read, &status );

    if ( status == enet_command_bad ) {
        printf( "> File %s nof found.\n", name );
        return enet_true;
    } else if ( status == enet_command_bad_name ) {
        printf( "> You must set your name with \"name\" command before using pull.\n" );
        return enet_true;
    } else if ( status != enet_command_ok ) {
        printf( "> Unknow error.\n" );
        return enet_true;
    }

    net_file_t file;
    memset( &file, 0x00, sizeof( net_file_t ) );

    if ( net_file_open( &file, enet_buffer_io_write, name ) == enet_false ) {
        printf( "> Can't create destination file\n" );
        return enet_true;
    }

    const uint8_t* src_buffer = (uint8_t*)net_buffer_get_raw( context->buffer_read.buffer ) + 2 * sizeof( uint32_t );
    uint32_t file_length = 0;
    
    net_buffer_io_read_uint32( &context->buffer_read, &file_length );
    
    fwrite( src_buffer, sizeof( uint8_t ), file_length, file.file );

    net_file_close( &file );

    printf( "> File %s writing completed.\n", name );

    return enet_true;
}

enet_booleans client_name( client_context_t* context, net_buffer_t* input_buffer ) {
    const uint32_t cmd_length = 5;
    const uint32_t length = (uint32_t)strlen( (const char*)input_buffer->data );

    if ( length <= cmd_length ) {
        printf( "> You can't set you user name as empty.\n" );
        return enet_true;
    }

    if ( net_buffer_create( &context->decypher_buffer, 2 * sizeof( uint32_t ) + length ) == enet_false )
        return enet_false;

    const char* name = (const char*)input_buffer->data + cmd_length;

    net_buffer_io_reset( &context->buffer_write );
    net_buffer_io_write_uint32( &context->buffer_write, enet_command_name );
    net_buffer_io_write_uint32( &context->buffer_write, length - cmd_length );
    net_buffer_io_write_raw( &context->buffer_write, name, length, NULL );

    if (
        net_send( context ) == enet_false ||
        net_recv( context ) == enet_false
    ) {
        printf( "> Connection lost.\n" );
        return enet_false;
    }
    
    uint32_t status;
    net_buffer_io_read_uint32( &context->buffer_read, &status );

    if ( status == enet_command_ok ) {
        printf( "> Nammed : %s.\n", name );
        return enet_true;
    }
    
    printf( "> Naming failed.\n" );

    return enet_false;
}

void print_help( ) {
    printf( "> Commands :\n" );
    printf( "> name user_name -> Set the current user name, must be the first command.\n" );
    printf( "> send file_name -> Send file to the server for the current user.\n" );
    printf( "> list -> List all file for the current user\n" );
    printf( "> pull file_name -> Pull a file from the server for the current user.\n" );
}

int main( int argc, char** argv ) {
    char* address = LOCAL_SERVER;
    uint32_t port = LOCAL_PORT;
    uint32_t crypto_seed = (uint32_t)time( NULL );

    client_context_t context;
    memset( &context, 0x00, sizeof( client_context_t ) );

    parse_arguments( argc, argv, &address, &port, &crypto_seed );

    net_crypto_init_seed( crypto_seed );

    if ( connect_to( &context, address, port ) == enet_false )
        return -1;

    print_help( );
    printf(
        "RSA Keys [\n\tClient Private : { %lu - %lu }\n\tServer Public : { %lu - %lu }\n]\n",
        context.client_private.exponent, context.client_private.modulus,
        context.server_public.exponent, context.server_public.modulus
    );

    if ( net_buffer_create( &context.cypher_buffer, 16 * sizeof(int32_t) ) == enet_false ) {
        net_socket_destroy( &context.socket );
        return -1;
    }
    
    if ( net_buffer_create( &context.decypher_buffer, 16 * sizeof(int32_t) ) == enet_false ) {
        net_buffer_destroy( &context.cypher_buffer );
        net_socket_destroy( &context.socket );
        return -1;
    }

    net_buffer_t input_buffer;
    memset( &input_buffer, 0x00, sizeof( net_buffer_t ) );

    if ( net_buffer_create( &input_buffer, 256 * sizeof( char ) ) == enet_false ) {
        net_buffer_destroy( &context.cypher_buffer );
        net_buffer_destroy( &context.decypher_buffer );
        net_socket_destroy( &context.socket );
        return -1;
    }

    enet_booleans is_running  = enet_true;
    context.buffer_write = net_buffer_io_acquire( &context.decypher_buffer, enet_buffer_io_write );
    context.buffer_read  = net_buffer_io_acquire( &context.cypher_buffer, enet_buffer_io_read );

    while ( is_running == enet_true ) {
        printf( "> " );
        if ( net_read_input( &input_buffer, enet_true ) == enet_false )
            break;

        if ( net_buffer_contain( &input_buffer, "help" ) ) {
            print_help( );
            continue;
        }

        enet_command_t command = parse_command( &input_buffer );

        net_buffer_io_reset( &context.buffer_write );
        net_buffer_io_reset( &context.buffer_read );
        net_buffer_io_write_uint32( &context.buffer_write, command );

        switch ( command ) {
            case enet_command_quit :
                net_send( &context );
                is_running = enet_false; 
                break;

            case enet_command_send : is_running = client_send( &context, &input_buffer ); break;
            case enet_command_list : is_running = client_list( &context ); break;
            case enet_command_pull : is_running = client_pull( &context, &input_buffer ); break;
            case enet_command_name : is_running = client_name( &context, &input_buffer ); break;

            default : break;
        }
    }

    net_buffer_destroy( &context.cypher_buffer );
    net_buffer_destroy( &context.decypher_buffer );
    net_buffer_destroy( &input_buffer );
    net_socket_destroy( &context.socket );

    return 0;
}
