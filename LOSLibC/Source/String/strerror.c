// LOSLibC/Source/String/strerror.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

typedef struct
{
    int         Number;
    CONST char *English;
    CONST char *Korean;
} ERROR_MESSAGE;

STATIC CONST ERROR_MESSAGE Messages[] = {
    {0, "Success", "성공"}, {EPERM, "Operation not permitted", "허용되지 않은 작업"}, {ENOENT, "No such file or directory", "그런 파일이나 디렉터리가 없습니다"}, {ESRCH, "No such process", "그런 프로세스가 없습니다"}, {EINTR, "Interrupted system call", "시스템 호출이 중단되었습니다"}, {EIO, "Input/output error", "입출력 오류"}, {ENXIO, "No such device or address", "그런 장치나 주소가 없습니다"}, {E2BIG, "Argument list too long", "인자 목록이 너무 깁니다"}, {ENOEXEC, "Exec format error", "실행 파일 형식 오류"}, {EBADF, "Bad file descriptor", "잘못된 파일 디스크립터"}, {ECHILD, "No child processes", "자식 프로세스가 없습니다"}, {EAGAIN, "Resource temporarily unavailable", "자원을 일시적으로 사용할 수 없습니다"}, {ENOMEM, "Cannot allocate memory", "메모리를 할당할 수 없습니다"}, {EACCES, "Permission denied", "권한이 없습니다"}, {EFAULT, "Bad address", "잘못된 주소"}, {ENOTBLK, "Block device required", "블록 장치가 필요합니다"}, {EBUSY, "Device or resource busy", "장치나 자원이 사용 중입니다"}, {EEXIST, "File exists", "파일이 이미 있습니다"}, {EXDEV, "Invalid cross-device link", "장치 간 연결이 허용되지 않습니다"}, {ENODEV, "No such device", "그런 장치가 없습니다"}, {ENOTDIR, "Not a directory", "디렉터리가 아닙니다"}, {EISDIR, "Is a directory", "디렉터리입니다"}, {EINVAL, "Invalid argument", "잘못된 인자"}, {ENFILE, "Too many open files in system", "시스템에 열린 파일이 너무 많습니다"}, {EMFILE, "Too many open files", "열린 파일이 너무 많습니다"}, {ENOTTY, "Inappropriate ioctl for device", "장치에 적합하지 않은 ioctl입니다"}, {ETXTBSY, "Text file busy", "실행 파일이 사용 중입니다"}, {EFBIG, "File too large", "파일이 너무 큽니다"}, {ENOSPC, "No space left on device", "장치에 남은 공간이 없습니다"}, {ESPIPE, "Illegal seek", "탐색할 수 없습니다"}, {EROFS, "Read-only file system", "읽기 전용 파일 시스템"}, {EMLINK, "Too many links", "링크가 너무 많습니다"}, {EPIPE, "Broken pipe", "파이프 연결이 끊어졌습니다"}, {EDOM, "Numerical argument out of domain", "수치 인자가 정의역을 벗어났습니다"}, {ERANGE, "Numerical result out of range", "수치 결과가 범위를 벗어났습니다"}, {EDEADLK, "Resource deadlock avoided", "자원 교착 상태를 방지했습니다"}, {ENAMETOOLONG, "File name too long", "파일 이름이 너무 깁니다"}, {ENOLCK, "No locks available", "사용 가능한 잠금이 없습니다"}, {ENOSYS, "Function not implemented", "구현되지 않은 기능"}, {ENOTEMPTY, "Directory not empty", "디렉터리가 비어 있지 않습니다"}, {ELOOP, "Too many levels of symbolic links", "심볼릭 링크가 너무 깊습니다"}, {EOVERFLOW, "Value too large for defined data type", "값이 자료형의 범위를 벗어났습니다"}, {EILSEQ, "Invalid or incomplete multibyte or wide character", "잘못되었거나 불완전한 문자"}, {EOPNOTSUPP, "Operation not supported", "지원하지 않는 작업"}, {ENOTSOCK, "Socket operation on non-socket", "소켓이 아닌 대상에 대한 소켓 작업"}, {EADDRINUSE, "Address already in use", "주소가 이미 사용 중입니다"}, {EADDRNOTAVAIL, "Cannot assign requested address", "요청한 주소를 할당할 수 없습니다"}, {ENETDOWN, "Network is down", "네트워크가 동작하지 않습니다"}, {ENETUNREACH, "Network is unreachable", "네트워크에 도달할 수 없습니다"}, {ECONNRESET, "Connection reset by peer", "상대방이 연결을 초기화했습니다"}, {ENOBUFS, "No buffer space available", "사용 가능한 버퍼 공간이 없습니다"}, {ENOTCONN, "Transport endpoint is not connected", "연결되어 있지 않습니다"}, {ETIMEDOUT, "Connection timed out", "연결 시간이 초과되었습니다"}, {ECONNREFUSED, "Connection refused", "연결이 거부되었습니다"}, {EHOSTUNREACH, "No route to host", "호스트에 도달할 수 없습니다"}, {ECANCELED, "Operation canceled", "작업이 취소되었습니다"},
};

char *strerror(int Error)
{
    BOOLEAN Korean = strcmp(setlocale(LC_MESSAGES, NULL), "ko_KR.UTF-8") == 0;

    for (UINTN Index = 0; Index < sizeof(Messages) / sizeof(Messages[0]); Index++)
    {
        if (Messages[Index].Number == Error)
        {
            return (char *) (Korean ? Messages[Index].Korean : Messages[Index].English);
        }
    }

    STATIC char Unknown[64];

    strcpy(Unknown, Korean ? "알 수 없는 오류 " : "Unknown error ");

    UINTN Length = strlen(Unknown);
    INT64 Number = Error;

    if (Number < 0)
    {
        Unknown[Length++] = '-';
        Number = -Number;
    }

    char  Digits[16];
    UINTN Count = 0;

    do
    {
        Digits[Count++] = '0' + Number % 10;
        Number /= 10;
    } while (Number != 0);

    while (Count != 0)
    {
        Unknown[Length++] = Digits[--Count];
    }

    Unknown[Length] = 0;

    return Unknown;
}
