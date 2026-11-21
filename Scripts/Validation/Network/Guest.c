// Scripts/Validation/Network/Guest.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLib/TypeInfo.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

STATIC INT32 Wait(INT32 FD, INT16 Events)
{
    struct pollfd Item = {FD, Events, 0};
    int Result = poll(&Item, 1, 15000);
    if (Result != 1 || (Item.revents & Events) == 0) printf("SOCKET_WAIT fd=%d events=%d result=%d returned=%d errno=%d\n", FD, Events, Result, Item.revents, errno);
    return Result == 1 && (Item.revents & Events) != 0 ? 0 : -1;
}

STATIC INT32 Exchange(INT32 FD, CONST char *Request, CONST char *Reply)
{
    UINTN Position = 0;
    UINTN Length   = strlen(Request);
    while (Position < Length)
    {
        if (Wait(FD, POLLOUT) != 0)
            return -1;
        ssize_t Count = send(FD, Request + Position, Length - Position, MSG_NOSIGNAL);
        if (Count < 0 && errno == EAGAIN)
            continue;
        if (Count <= 0)
            return -1;
        Position += (UINTN) Count;
    }
    char Buffer[128] = {0};
    Position         = 0;
    Length           = strlen(Reply);
    while (Position < Length)
    {
        if (Wait(FD, POLLIN) != 0)
            return -1;
        ssize_t Count = read(FD, Buffer + Position, Length - Position);
        if (Count < 0 && errno == EAGAIN)
            continue;
        if (Count <= 0)
            return -1;
        Position += (UINTN) Count;
    }
    return memcmp(Buffer, Reply, Length) == 0 ? 0 : -1;
}

#define CHECK(Condition) \
    if (!(Condition)) \
    { \
        Result = __LINE__; \
        goto Done; \
    }

INT32 main(VOID)
{
    INT64 Result = 0;
    open("/dev/lsh1", O_RDONLY);
    open("/dev/lsh1", O_WRONLY);
    open("/dev/lsh1", O_WRONLY);
    open("/dev/lsh1-master", O_RDWR);
    setvbuf(stdout, NULL, _IONBF, 0);
    INT32 Server = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    CHECK(Server >= 0);
    struct sockaddr_in Address = {.sin_family = AF_INET, .sin_port = __builtin_bswap16(18080)};
    CHECK(bind(Server, (struct sockaddr *) &Address, sizeof(Address)) == 0);
    CHECK(listen(Server, 4) == 0);
    puts("SOCKET_SERVER_READY");
    CHECK(Wait(Server, POLLIN) == 0);
    socklen_t AddressLength = sizeof(Address);
    INT32     Client        = accept4(Server, (struct sockaddr *) &Address, &AddressLength, SOCK_NONBLOCK | SOCK_CLOEXEC);
    CHECK(Client >= 0 && AddressLength == sizeof(Address));
    CHECK(Exchange(Client, "LineOS TCP PASS\n", "host-client\n") == 0);
    close(Client);
    close(Server);
    Client = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    CHECK(Client >= 0);
    Address.sin_addr.s_addr = __builtin_bswap32(0x0a000202);
    Address.sin_port        = __builtin_bswap16(19391);
    CHECK(connect(Client, (struct sockaddr *) &Address, sizeof(Address)) == -1 && errno == EINPROGRESS);
    CHECK(Wait(Client, POLLOUT) == 0);
    INT32     Error       = -1;
    socklen_t ErrorLength = sizeof(Error);
    CHECK(getsockopt(Client, SOL_SOCKET, SO_ERROR, &Error, &ErrorLength) == 0 && Error == 0);
    CHECK(Exchange(Client, "guest-client\n", "host-reply\n") == 0);
    close(Client);
Done:
    printf("SOCKET_GUEST_%s line=%ld errno=%d\n", Result == 0 ? "PASS" : "FAIL", Result, errno);
    INT32 FD = open(Result == 0 ? "/socket-PASS" : "/socket-FAIL", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (FD >= 0)
    {
        write(FD, &Result, sizeof(Result));
        close(FD);
    }
    for (;;)
    {
        struct pollfd Empty = {-1, 0, 0};
        poll(&Empty, 1, 1000);
    }
}
