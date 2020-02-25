#ifndef SDL_NET_EXT_H
#define SDL_NET_EXT_H

#include <SDL2/SDL_net.h>

/* TCP */
typedef struct
{
    Uint8 *data;
    int len;
    int maxlen;
} TCPpacket;

TCPpacket *SDLNet_TCP_AllocPacket(int size);
int SDLNet_TCP_SendExt(TCPsocket socket, void *data, int len);
int SDLNet_TCP_RecvExt(TCPsocket socket, TCPpacket *packet);
void SDLNet_TCP_FreePacket(TCPpacket *packet);

/* UDP */
UDPpacket *SDLNet_UDP_AllocPacket(int size);
int SDLNet_UDP_SendExt(UDPsocket socket, UDPpacket *packet, IPaddress address, void *data, int len);
int SDLNet_UDP_RecvExt(UDPsocket socket, UDPpacket *packet);
void SDLNet_UDP_FreePacket(UDPpacket *packet);

#endif
