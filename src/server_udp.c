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

int main(int argc, char **argv) {
    net_unused(argc);
    net_unused(argv);

    net_socket_t socket;
    memset(&socket, 0x00, sizeof(net_socket_t));

    if ( net_socket_create_server( &socket, 25565, 1,enet_socket_udp ) == enet_false )
        return -1;
    
    net_buffer_t msg = net_buffer_immutable( "Hello peer !" );
    net_buffer_t buffer;
    memset(&buffer, 0x00, sizeof(net_buffer_t));

    if ( net_buffer_create(&buffer, 16 * sizeof(int32_t)) == enet_false )
        return -1;

    while (enet_true)
    {
        if ( net_socket_recv(&socket, &buffer) == enet_true )
            printf( "%s\n", (const char *)buffer.data);

        net_socket_send(&socket, &msg);
    }

    net_socket_destroy(&socket);

    return 0;
}
