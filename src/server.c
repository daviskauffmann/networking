#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdbool.h>
#include <stdio.h>

#include "client.h"
#include "data.h"
#include "SDL_net_ext.h"

#define SERVER_PORT 1000
#define MAX_CLIENTS 8

// TODO: handle timeouts on clients to automatically disconnect them
struct client
{
    int id;
    TCPsocket socket;
    IPaddress udp_address;
};

static struct client clients[MAX_CLIENTS];

static int count_clients(void)
{
    int num_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].id != -1)
        {
            num_clients++;
        }
    }
    return num_clients;
}

static void broadcast(void *data, int len, int exclude_id)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].id != -1 && clients[i].id != exclude_id)
        {
            SDLNet_TCP_SendExt(clients[i].socket, data, len);
        }
    }
}

int server_main(int argc, char *argv[])
{
    // init SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return 1;
    }

    // init SDL_net
    if (SDLNet_Init() != 0)
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    // setup server info
    IPaddress server_address;
    if (SDLNet_ResolveHost(&server_address, INADDR_ANY, SERVER_PORT))
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    // open TCP socket
    TCPsocket tcp_socket = SDLNet_TCP_Open(&server_address);
    if (!tcp_socket)
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    {
        const char *host = SDLNet_ResolveIP(&server_address);
        unsigned short port = SDLNet_Read16(&server_address.port);
        printf("TCP: Listening on %s:%i", host, port);
    }

    // allocate TCP packet
    TCPpacket *tcp_packet = SDLNet_TCP_AllocPacket(PACKET_SIZE);
    if (!tcp_packet)
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    // open UDP socket
    UDPsocket udp_socket = SDLNet_UDP_Open(SERVER_PORT);
    if (!udp_socket)
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    // allocate UDP packet
    UDPpacket *udp_packet = SDLNet_UDP_AllocPacket(PACKET_SIZE);
    if (!udp_packet)
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    // allocate socket set
    SDLNet_SocketSet socket_set = SDLNet_AllocSocketSet(MAX_CLIENTS + 2);
    if (!socket_set)
    {
        printf("Error: %s\n", SDLNet_GetError());
        return 1;
    }

    // add TCP and UDP sockets to socket set
    SDLNet_TCP_AddSocket(socket_set, tcp_socket);
    SDLNet_UDP_AddSocket(socket_set, udp_socket);

    // setup client list
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].id = -1;
        clients[i].socket = NULL;
    }

    // main loop
    bool quit = false;
    while (!quit)
    {
        // handle network events
        while (SDLNet_CheckSockets(socket_set, 0) > 0)
        {
            // check activity on the server
            if (SDLNet_SocketReady(tcp_socket))
            {
                // accept new clients
                TCPsocket socket = SDLNet_TCP_Accept(tcp_socket);
                if (socket)
                {
                    // search for an empty client
                    int client_id = -1;
                    for (int i = 0; i < MAX_CLIENTS; i++)
                    {
                        if (clients[i].id == -1)
                        {
                            client_id = i;
                            break;
                        }
                    }
                    if (client_id != -1)
                    {
                        // get socket info
                        IPaddress *address = SDLNet_TCP_GetPeerAddress(socket);
                        const char *host = SDLNet_ResolveIP(address);
                        unsigned short port = SDLNet_Read16(&address->port);
                        printf("Connected to client %s:%d\n", host, port);

                        // initialize the client
                        clients[client_id].id = client_id;
                        clients[client_id].socket = socket;

                        // add to the socket list
                        SDLNet_TCP_AddSocket(socket_set, clients[client_id].socket);

                        // send the client their info
                        {
                            struct id_data id_data = id_data_create(DATA_CONNECT_OK, clients[client_id].id);
                            SDLNet_TCP_SendExt(socket, &id_data, sizeof(id_data));
                        }

                        // inform other clients
                        struct id_data id_data = id_data_create(DATA_CONNECT_BROADCAST, clients[client_id].id);
                        broadcast(&id_data, sizeof(id_data), clients[client_id].id);

                        // log the current number of clients
                        printf("There are %d clients connected\n", count_clients());
                    }
                    else
                    {
                        printf("A client tried to connect, but the server is full\n");

                        // send client a full server message
                        struct data data = data_create(DATA_CONNECT_FULL);
                        SDLNet_TCP_SendExt(socket, &data, sizeof(data));
                    }
                }
            }

            // check activity on each client
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].id != -1)
                {
                    // handle TCP messages
                    if (SDLNet_SocketReady(clients[i].socket))
                    {
                        if (SDLNet_TCP_RecvExt(clients[i].socket, tcp_packet) == 1)
                        {
                            struct data *data = (struct data *)tcp_packet->data;
                            switch (data->type)
                            {
                            case DATA_DISCONNECT_REQUEST:
                            {
                                // get socket info
                                IPaddress *address = SDLNet_TCP_GetPeerAddress(clients[i].socket);
                                const char *host = SDLNet_ResolveIP(address);
                                unsigned short port = SDLNet_Read16(&address->port);
                                printf("Disconnecting from client %s:%d\n", host, port);

                                // inform other clients
                                struct id_data id_data = id_data_create(DATA_DISCONNECT_BROADCAST, clients[i].id);
                                broadcast(&id_data, sizeof(id_data), clients[i].id);

                                // close the TCP connection
                                SDLNet_TCP_DelSocket(socket_set, clients[i].socket);
                                SDLNet_TCP_Close(clients[i].socket);

                                // uninitialize the client
                                clients[i].id = -1;
                                clients[i].socket = NULL;

                                // log the current number of clients
                                printf("There are %d clients connected\n", count_clients());
                            }
                            break;
                            case DATA_CHAT_REQUEST:
                            {
                                struct chat_data *chat_data = (struct chat_data *)data;
                                printf("Client %d: %s\n", chat_data->id, chat_data->message);

                                // relay to other clients
                                struct chat_data chat_data2 = chat_data_create(DATA_CHAT_BROADCAST, chat_data->id, chat_data->message);
                                broadcast(&chat_data2, sizeof(chat_data2), clients[i].id);
                            }
                            break;
                            default:
                            {
                                printf("TCP: Unknown packet type\n");
                            }
                            break;
                            }
                        }
                    }
                }
            }

            // handle UDP messages
            if (SDLNet_SocketReady(udp_socket))
            {
                if (SDLNet_UDP_RecvExt(udp_socket, udp_packet) == 1)
                {
                    struct data *data = (struct data *)udp_packet->data;
                    switch (data->type)
                    {
                    case DATA_UDP_CONNECT_REQUEST:
                    {
                        struct id_data *id_data = (struct id_data *)data;
                        printf("Saving UDP info of client %d\n", id_data->id);

                        // save the UDP address
                        clients[id_data->id].udp_address = udp_packet->address;
                    }
                    break;
                    case DATA_MOUSEDOWN_REQUEST:
                    {
                        struct mouse_data *mouse_data = (struct mouse_data *)data;
                        printf("Client %d mouse down: (%d, %d)\n", mouse_data->id, mouse_data->x, mouse_data->y);
                    }
                    break;
                    default:
                    {
                        printf("UDP: Unknown packet type\n");
                    }
                    break;
                    }
                }
            }
        }
    }

    // close clients
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].id != -1)
        {
            SDLNet_TCP_DelSocket(socket_set, clients[i].socket);
            SDLNet_TCP_Close(clients[i].socket);

            clients[i].id = -1;
            clients[i].socket = NULL;
        }
    }

    // close SDL_net
    SDLNet_UDP_DelSocket(socket_set, udp_socket);
    SDLNet_TCP_DelSocket(socket_set, tcp_socket);
    SDLNet_FreeSocketSet(socket_set);
    SDLNet_UDP_FreePacket(udp_packet);
    SDLNet_UDP_Close(udp_socket);
    SDLNet_TCP_FreePacket(tcp_packet);
    SDLNet_TCP_Close(tcp_socket);
    SDLNet_Quit();

    // close SDL
    SDL_Quit();

    return 0;
}
