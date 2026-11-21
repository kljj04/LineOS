
// UserPrograms/Shell/Commands/ncecho/ncecho.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <Userspace/TypeInfo.h>

#define NCECHO_PORT        52222
#define NCECHO_BACKLOG     8
#define NCECHO_BUFFER_SIZE 4096

STATIC INT32 WriteAll(INT32 FD, CONST VOID *Buffer, UINT64 Length)
{
    CONST UINT8 *Data   = (CONST UINT8 *) Buffer;
    UINT64       Offset = 0;

    while (Offset < Length)
    {
        ssize_t Written = write(FD, Data + Offset, Length - Offset);

        if (Written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (Written == 0)
        {
            return -1;
        }

        Offset += (UINT64) Written;
    }

    return 0;
}

STATIC VOID HandleClient(INT32 ClientFD, CONST struct sockaddr_in *Address)
{
    UINT8 Buffer[NCECHO_BUFFER_SIZE];

    printf("[+] Client connected: %s:%u\n", inet_ntoa(Address->sin_addr), ntohs(Address->sin_port));

    while (TRUE)
    {
        SSIZE Received = read(ClientFD, Buffer, sizeof(Buffer));

        if (Received < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("read");
            break;
        }

        if (Received == 0)
        {
            break;
        }

        printf("%.*s\n", (INT32) Received, Buffer);

        if (WriteAll(ClientFD, Buffer, (UINT64) Received) != 0)
        {
            perror("write");
            break;
        }
    }

    close(ClientFD);
    printf("[-] Client disconnected\n");
}

INT32 main(VOID)
{
    INT32              ServerFD;
    INT32              ClientFD;
    INT32              Opt = 1;
    struct sockaddr_in ServerAddress;

    signal(SIGPIPE, SIG_IGN);

    ServerFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (ServerFD < 0)
    {
        perror("socket");
        return 1;
    }

    if (setsockopt(ServerFD, SOL_SOCKET, SO_REUSEADDR, &Opt, sizeof(Opt)) != 0)
    {
        perror("setsockopt");
        close(ServerFD);
        return 1;
    }

    memset(&ServerAddress, 0, sizeof(ServerAddress));

    ServerAddress.sin_family      = AF_INET;
    ServerAddress.sin_port        = htons(NCECHO_PORT);
    ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ServerFD, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)) != 0)
    {
        perror("bind");
        close(ServerFD);
        return 1;
    }

    if (listen(ServerFD, NCECHO_BACKLOG) != 0)
    {
        perror("listen");
        close(ServerFD);
        return 1;
    }

    printf("[+] NCEcho listening on port %u\n", NCECHO_PORT);

    while (TRUE)
    {
        struct sockaddr_in ClientAddress;
        socklen_t          ClientLength = sizeof(ClientAddress);

        ClientFD = accept(ServerFD, (struct sockaddr *) &ClientAddress, &ClientLength);

        if (ClientFD < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("accept");
            continue;
        }

        HandleClient(ClientFD, &ClientAddress);
    }

    close(ServerFD);
    return 0;
}