#include "client.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdio.h>
#include <stdbool.h>

#include "data.h"
#include "SDL_net_ext.h"

#define WINDOW_TITLE "Client"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 1000

int client_main(int argc, char *argv[])
{
    // init SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("Error: %s\n", SDL_GetError());

        return 1;
    }

    // create window
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0);

    if (!window)
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

    if (SDLNet_ResolveHost(&server_address, SERVER_HOST, SERVER_PORT))
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

        printf("TCP: Connected to %s:%i", host, port);
    }

    // allocate TCP packet
    TCPpacket *tcp_packet = SDLNet_TCP_AllocPacket(PACKET_SIZE);

    if (!tcp_packet)
    {
        printf("Error: %s\n", SDLNet_GetError());

        return 1;
    }

    // open UDP socket
    UDPsocket udp_socket = SDLNet_UDP_Open(0);

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
    SDLNet_SocketSet socket_set = SDLNet_AllocSocketSet(2);

    if (!socket_set)
    {
        printf("Error: %s\n", SDLNet_GetError());

        return 1;
    }

    // add TCP and UDP sockets to socket set
    SDLNet_TCP_AddSocket(socket_set, tcp_socket);
    SDLNet_UDP_AddSocket(socket_set, udp_socket);

    // keep track of unique client ID given by server
    int client_id = -1;

    // check the server's response to the connection
    if (SDLNet_TCP_RecvExt(tcp_socket, tcp_packet) == 1)
    {
        struct data *data = (struct data *)tcp_packet->data;

        switch (data->type)
        {
        case DATA_CONNECT_OK:
        {
            struct id_data *id_data = (struct id_data *)data;

            printf("Server assigned ID: %d\n", id_data->id);

            client_id = id_data->id;
        }
        break;
        case DATA_CONNECT_FULL:
        {
            printf("Error: Server is full\n");

            return 1;
        }
        break;
        default:
        {
            printf("Error: Unknown server response\n");

            return 1;
        }
        break;
        }
    }

    // make a UDP "connection" to the server
    {
        struct id_data id_data = id_data_create(DATA_UDP_CONNECT_REQUEST, client_id);
        SDLNet_UDP_SendExt(udp_socket, udp_packet, server_address, &id_data, sizeof(id_data));
    }

    // main loop
    bool quit = false;
    while (!quit)
    {
        // get keyboard input
        int num_keys;
        const unsigned char *keys = SDL_GetKeyboardState(&num_keys);

        // get mouse input
        int mouse_x, mouse_y;
        unsigned int mouse = SDL_GetMouseState(&mouse_x, &mouse_y);

        // handle events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_KEYDOWN:
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_F4:
                {
                    if (keys[SDL_SCANCODE_LALT])
                    {
                        quit = true;
                    }
                }
                break;
                case SDLK_RETURN:
                {
                    struct chat_data chat_data = chat_data_create(DATA_CHAT_REQUEST, client_id, "Hello, World!");
                    SDLNet_TCP_SendExt(tcp_socket, &chat_data, sizeof(chat_data));
                }
                break;
                }
            }
            break;
            case SDL_MOUSEBUTTONDOWN:
            {
                struct mouse_data mouse_data = mouse_data_create(DATA_MOUSEDOWN_REQUEST, client_id, event.button.x, event.button.y);
                SDLNet_UDP_SendExt(udp_socket, udp_packet, server_address, &mouse_data, sizeof(mouse_data));
            }
            break;
            case SDL_QUIT:
            {
                quit = true;
            }
            break;
            }
        }

        // handle network events
        while (SDLNet_CheckSockets(socket_set, 0) > 0)
        {
            // handle TCP messages
            if (SDLNet_SocketReady(tcp_socket))
            {
                // accept new clients
                if (SDLNet_TCP_RecvExt(tcp_socket, tcp_packet) == 1)
                {
                    struct data *data = (struct data *)tcp_packet->data;

                    switch (data->type)
                    {
                    case DATA_CONNECT_BROADCAST:
                    {
                        struct id_data *id_data = (struct id_data *)data;

                        printf("Client with ID %d has joined\n", id_data->id);
                    }
                    break;
                    case DATA_DISCONNECT_BROADCAST:
                    {
                        struct id_data *id_data = (struct id_data *)data;

                        printf("Client with ID %d has disconnected\n", id_data->id);
                    }
                    break;
                    case DATA_CHAT_BROADCAST:
                    {
                        struct chat_data *chat_data = (struct chat_data *)data;

                        printf("Client %d: %s\n", chat_data->id, chat_data->message);
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

            // handle UDP messages
            if (SDLNet_SocketReady(udp_socket))
            {
                if (SDLNet_UDP_RecvExt(udp_socket, udp_packet) == 1)
                {
                    struct data *data = (struct data *)udp_packet->data;

                    switch (data->type)
                    {
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

    // send a disconnect message
    {
        struct data data = data_create(DATA_DISCONNECT_REQUEST);
        SDLNet_TCP_SendExt(tcp_socket, &data, sizeof(data));
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
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
