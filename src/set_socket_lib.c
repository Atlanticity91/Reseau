/************************************************************************************************
 * 
 *  _   _      _     _____            _        _
 * | \ | |    | |   /  ___|          | |      | |
 * |  \| | ___| |_  \ `--.  ___   ___| | _____| |_
 * | . ` |/ _ \ __|  `--. \/ _ \ / __| |/ / _ \ __|
 * | |\  |  __/ |_  /\__/ / (_) | (__|   <  __/ |_
 * \_| \_/\___|\__| \____/ \___/ \___|_|\_\___|\__|
 * 
 * @author ALVES Quentin
 * @license MIT
 * 
 ***********************************************************************************************/

#include "net_socket_lib.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
// SOCKET
/////////////////////////////////////////////////////////////////////////////////////////////////
enet_booleans net_socket_is_valid( const net_socket_t* socket ) {
    if ( socket == NULL || socket->socket == -1 )
        return enet_false;

    return enet_true;
}

enet_booleans net_socket_send( const net_socket_t* socket, net_buffer_t* buffer ) {
    assert( net_socket_is_valid( socket ) == enet_true );
    assert( net_buffer_is_valid( buffer ) == enet_true );

    return enet_false;
}

enet_booleans net_socket_recv( const net_socket_t* socket, net_buffer_t* buffer ) {
    assert( net_socket_is_valid( socket ) == enet_true );
    assert( net_buffer_is_valid( buffer ) == enet_true );

    return enet_false;
}
