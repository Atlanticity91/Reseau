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

#ifndef _NET_GLOBALS_H_
#define _NET_GLOBALS_H_

#define TCP_MAX_CLIENT_COUNT 4
#define LOCAL_SERVER "127.0.0.1"
#define LOCAL_PORT 25565

typedef enum enet_command_t {
    enet_command_quit = 1,
    enet_command_send,
    enet_command_list,
    enet_command_pull,
    enet_command_name,
    enet_command_ok,
    enet_command_bad,
    enet_command_bad_name
} enet_command_t;

#endif /* !_NET_GLOBALS_H_ */
