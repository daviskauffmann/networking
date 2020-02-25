#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "server.h"

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("Options:\n");
            printf("  -h, --help\tPrint this message\n");
            printf("  -c, --client\tRun as client\n");
            printf("  -s, --server\tRun as server\n");
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--client") == 0)
        {
            return client_main(argc, argv);
        }
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0)
        {
            return server_main(argc, argv);
        }
    }

    return 0;
}
