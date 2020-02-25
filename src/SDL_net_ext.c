#include "SDL_net_ext.h"

#include <SDL2/SDL_net.h>
#include <stdio.h>

/* TCP */
TCPpacket *SDLNet_TCP_AllocPacket(int size)
{
    TCPpacket *packet = malloc(sizeof(TCPpacket));

    if (!packet)
    {
        SDLNet_SetError("Couldn't allocate packet");

        return NULL;
    }

    packet->data = malloc(size);
    packet->maxlen = size;

    return packet;
}

int SDLNet_TCP_SendExt(TCPsocket socket, void *data, int len)
{
    IPaddress *address = SDLNet_TCP_GetPeerAddress(socket);
    const char *host = SDLNet_ResolveIP(address);
    unsigned short port = SDLNet_Read16(&address->port);

    printf("TCP: Sending %d bytes to %s:%d\n", len, host, port);

    return SDLNet_TCP_Send(socket, data, len);
}

int SDLNet_TCP_RecvExt(TCPsocket socket, TCPpacket *packet)
{
    packet->len = SDLNet_TCP_Recv(socket, packet->data, packet->maxlen);

    if (packet->len > 0)
    {
        IPaddress *address = SDLNet_TCP_GetPeerAddress(socket);
        const char *host = SDLNet_ResolveIP(address);
        unsigned short port = SDLNet_Read16(&address->port);

        printf("TCP: Received %d bytes from %s:%d\n", packet->len, host, port);

        return 1;
    }

    return 0;
}

void SDLNet_TCP_FreePacket(TCPpacket *packet)
{
    free(packet->data);
    free(packet);
}

/* UDP */
UDPpacket *SDLNet_UDP_AllocPacket(int size)
{
    return SDLNet_AllocPacket(size);
}

int SDLNet_UDP_SendExt(UDPsocket socket, UDPpacket *packet, IPaddress address, void *data, int len)
{
    packet->address = address;
    packet->data = (Uint8 *)data;
    packet->len = len;

    const char *host = SDLNet_ResolveIP(&packet->address);
    unsigned short port = SDLNet_Read16(&packet->address.port);

    printf("UDP: Sending %d bytes to %s:%d\n", packet->len, host, port);

    return SDLNet_UDP_Send(socket, -1, packet);
}

int SDLNet_UDP_RecvExt(UDPsocket socket, UDPpacket *packet)
{
    int recv = SDLNet_UDP_Recv(socket, packet);

    if (recv == 1)
    {
        const char *host = SDLNet_ResolveIP(&packet->address);
        unsigned short port = SDLNet_Read16(&packet->address.port);

        printf("UDP: Received %d bytes from %s:%d\n", packet->len, host, port);
    }

    return recv;
}

void SDLNet_UDP_FreePacket(UDPpacket *packet)
{
    SDLNet_FreePacket(packet);
}
