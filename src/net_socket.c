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

#define INVALID_SOCKET_DESCRIPTOR -1

/////////////////////////////////////////////////////////////////////////////////////////////////
// SOCKET
/////////////////////////////////////////////////////////////////////////////////////////////////
void net_socket_init( net_socket_t* socket ) {
    assert( socket != NULL );

    memset( socket, 0x00, sizeof( net_socket_t ) );

    socket->descriptor = INVALID_SOCKET_DESCRIPTOR;
}

enet_booleans net_socket_create(
    net_socket_t *target_socket,
    enet_socket_types type,
    int32_t port,
    enet_socket_protocoles protocole
) {
    assert( target_socket != NULL );

    if ( protocole == enet_socket_tcp )
        target_socket->descriptor = socket( AF_INET, SOCK_STREAM, 0 );
    else
        target_socket->descriptor = socket( AF_INET, SOCK_DGRAM, 0 );

    target_socket->type = type;
    target_socket->protocole = protocole;
    target_socket->address.sin_family = AF_INET;
    target_socket->address.sin_port = htons( port );

    if ( net_socket_is_valid( target_socket ) == enet_false ) {
        net_print_error( "Can't create socket descriptor for %p", target_socket );

        return enet_false;
    }

    return enet_true;
}

enet_booleans net_socket_bind( net_socket_t* socket, const uint32_t port ) {
    if ( bind( socket->descriptor, (struct sockaddr*)&socket->address, sizeof( struct sockaddr_in ) ) < 0 ) {
        net_socket_destroy( socket );
        net_print_error( "Can't bind socket %p on port %u", socket, port );

        return enet_false;
    }

    return enet_true;
}

enet_booleans net_socket_create_server(
    net_socket_t *socket,
    const int32_t port,
    const uint32_t max_client_count,
    const enet_socket_protocoles protocole
) {
    assert( max_client_count > 0 );
    
    if ( net_socket_create( socket, enet_socket_server, port, protocole ) == enet_false )
        return enet_false;

    socket->protocole = protocole;
    socket->address.sin_addr.s_addr = INADDR_ANY;

    if ( net_socket_bind( socket, port ) == enet_false )
        return enet_false;

    if ( net_socket_is( socket, enet_socket_udp ) == enet_true ) 
        return enet_true;
    
    if ( listen( socket->descriptor, max_client_count ) < 0 ) {
        net_print_error( "Can't listen on socket %p", socket );
        net_socket_destroy( socket );

        return enet_false;
    }

    const int flags = fcntl( socket->descriptor, F_GETFL, 0 );
    fcntl( socket->descriptor, F_SETFL, flags | O_NONBLOCK );
    
    return enet_true;
}

enet_booleans net_socket_connect( net_socket_t* socket, const char* address ) {
    if ( connect( socket->descriptor, (struct sockaddr*)&socket->address, sizeof(struct sockaddr_in) ) < 0 ) {
        net_socket_destroy( socket );
        net_print_error( "Socket %p can't connect to the server %s", socket, address );

        return enet_false;
    }

    return enet_true;
}

enet_booleans net_socket_create_client(
    net_socket_t* socket,
    const char* address,
    const int32_t port,
    const enet_socket_protocoles protocole
) {
    assert( address != NULL );

    if ( net_socket_create( socket, enet_socket_client, port, protocole ) == enet_false )
        return enet_false;

    if ( inet_pton( AF_INET, address, &socket->address.sin_addr ) <= 0 ) {
        net_socket_destroy( socket );
        net_print_error( "Invalid server address %s for socket %p", address, socket );

        return enet_false;
    }

    if ( net_socket_is( socket, enet_socket_tcp ) == enet_true )
        return net_socket_connect( socket, address );

    return enet_true;
}

enet_booleans net_socket_accept( const net_socket_t* server, net_socket_t* client ) {
    assert( net_socket_is_valid( server ) == enet_true );
    assert( client != NULL );

    if ( net_socket_is( server, enet_socket_udp ) == enet_true )
        return enet_false;

    socklen_t socket_size = (socklen_t)sizeof(struct sockaddr_in);

    client->type = enet_socket_client;
    client->descriptor = accept( server->descriptor, (struct sockaddr *)&client->address, &socket_size );

    return net_socket_is_valid( client );
}

enet_booleans net_socket_udp_send( net_socket_t* socket, net_buffer_t* buffer ) {
    const uint8_t *src_buffer = (const uint8_t*)net_buffer_get_raw( buffer );
    const ssize_t state = sendto( socket->descriptor, src_buffer, buffer->size, 0, &socket->address, sizeof(struct sockaddr_in) );

    if ( state > 0 )
        return enet_true;
    else if ( state < 0 )
        net_print_error( "Can't send data with socket %p", socket );

    return enet_false;
}

enet_booleans net_socket_tcp_send( net_socket_t* socket, net_buffer_t* buffer ) {
    uint32_t length = 0;

    while ( length < buffer->size ) {
        const uint8_t* src_buffer = net_buffer_get_raw( buffer ) + length;
        const ssize_t state = send( socket->descriptor, src_buffer, buffer->size - length, 0 );

        if ( state > 0 )
            length += (uint32_t)state;
        else if ( state == 0 )
            return enet_false;
        else {
            if ( errno == EINTR )
                continue;

            net_print_error( "Can't send data with socket %p", socket );

            return enet_false;
        }
    }

    return enet_true;
}

enet_booleans net_socket_send( net_socket_t *socket, net_buffer_t *buffer ) {
    if ( net_socket_is( socket, enet_socket_tcp ) == enet_true ) {
        net_buffer_t temp_buff;
        memset( &temp_buff, 0x00, sizeof( net_buffer_t ) );

        if ( net_buffer_create( &temp_buff, sizeof( uint32_t) ) == enet_false )
            return enet_false;

        net_buffer_io_t buffer_io = net_buffer_io_acquire( &temp_buff, enet_buffer_io_write );

        if ( 
            net_buffer_io_write_uint32( &buffer_io, buffer->size ) == enet_false ||
            net_socket_tcp_send( socket, &temp_buff ) == enet_false
        ) {
            net_buffer_destroy( &temp_buff);

            return enet_false;
        }

        net_buffer_destroy( &temp_buff);

        return net_socket_tcp_send( socket, buffer );
    }

    return net_socket_udp_send( socket, buffer );
}

enet_booleans net_socket_udp_recv( net_socket_t* socket, net_buffer_t* buffer ) {
    uint8_t *dst_buffer = net_buffer_get_raw( buffer );
    socklen_t address_size = (socklen_t)sizeof( struct sockaddr_in );

    const ssize_t state = recvfrom( socket->descriptor, dst_buffer, buffer->length, 0, (struct sockaddr*)&socket->address, &address_size );

    if ( state > 0 ) {
        const uint32_t size = (uint32_t)state;

        if ( size < buffer->length )
            buffer->size = size;
        else
            buffer->size = buffer->length;

        return enet_true;
    }

    if ( state < 0 )
        net_print_error( "Can't receive data with socket %p", socket );

    return enet_false;
}

enet_booleans net_socket_tcp_recv( net_socket_t* socket, net_buffer_t* buffer ) {
    uint32_t length = 0;

    while ( length < buffer->size ) {
        uint8_t* dst_buffer = net_buffer_get_raw( buffer ) + length;
        const ssize_t state = recv( socket->descriptor, dst_buffer, buffer->size - length, 0 );

        if ( state > 0 )
            length += (uint32_t)state;
        else if ( state == 0 )
            return enet_false;
        else {
            if ( errno == EINTR )
                continue;

            net_print_error( "Can't receive data from socket %p", socket );

            return enet_false;
        }
    }

    return enet_true;
}

enet_booleans net_socket_recv( net_socket_t* socket, net_buffer_t* buffer ) {
    if ( net_socket_is( socket, enet_socket_tcp ) == enet_true ) {
        net_buffer_resize( buffer, sizeof( uint32_t ) );

        net_buffer_io_t buffer_io = net_buffer_io_acquire( buffer, enet_buffer_io_read );

        if ( net_socket_tcp_recv( socket, buffer ) == enet_false ) {
            printf( "Can't receive message length\n" );

            return enet_false;
        }

        uint32_t length = 0;
        
        if ( net_buffer_io_read_uint32( &buffer_io, &length ) == enet_false ) {
            printf( "Can't read message length\n" );

            return enet_false;
        }
        
        if ( net_buffer_create( buffer, length ) == enet_false )
            return enet_false;

        net_buffer_resize( buffer, length);

        return net_socket_tcp_recv( socket, buffer );
    }

    return net_socket_udp_recv( socket, buffer );
}

enet_booleans net_socket_is_valid( const net_socket_t* socket ) {
    assert( socket != NULL );

    if ( socket->descriptor <= INVALID_SOCKET_DESCRIPTOR )
        return enet_false;

    return enet_true;
}

enet_booleans net_socket_is_type(
    const net_socket_t* socket,
    const enet_socket_types type
) {
    assert( net_socket_is_valid( socket ) == enet_true );

    if ( socket->type != type )
        return enet_false;

    return enet_true;
}

enet_booleans net_socket_is(
    const net_socket_t* socket,
    const enet_socket_protocoles protocole
) {
    assert( net_socket_is_valid( socket ) == enet_true );

    if ( socket->protocole != protocole )
        return enet_false;

    return enet_true;
}

void net_socket_destroy( net_socket_t* socket ) {
    assert( net_socket_is_valid( socket ) == enet_true );

    close( socket->descriptor );

    net_socket_init( socket );
}
