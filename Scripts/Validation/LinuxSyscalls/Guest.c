#include <LineOS/TypeInfo.h>
#include <Syscall/File/Stat.h>
#include <Syscall/Memory/Memory.h>
STATIC INT64 Call(UINT64 Number, UINT64 A, UINT64 B, UINT64 C, UINT64 D, UINT64 E, UINT64 F)
{
    register UINT64 R10 __asm__("r10") = D;
    register UINT64 R8 __asm__("r8")   = E;
    register UINT64 R9 __asm__("r9")   = F;
    INT64           Result;
    __asm__ volatile("syscall" : "=a"(Result) : "a"(Number), "D"(A), "S"(B), "d"(C), "r"(R10), "r"(R8), "r"(R9) : "rcx", "r11", "memory");
    return Result;
}
#define CHECK(Condition) \
    if (!(Condition)) \
    { \
        Result = __LINE__; \
        goto Done; \
    }
extern int getpid(void);

VOID _Start(VOID)
{
    INT64 Result = 0;
#ifdef TEST_LIBC
    CHECK(getpid() > 0);
#endif
    INT64 FD = Call(257, -100, (UINT64) "/mmap-fixture", 0, 0, 0, 0);
    CHECK(FD >= 0);
    UINT8 Buffer[32];
    CHECK(Call(17, FD, (UINT64) Buffer, sizeof(Buffer), 42, 0, 0) == sizeof(Buffer));
    CHECK(Buffer[0] == 42);
    UINT64 Address = Call(9, 0, 8192, 3, MAP_PRIVATE, FD, 0);
    CHECK((INT64) Address > 0);
    CHECK(((UINT8 *) Address)[42] == 42);
    CHECK(((UINT8 *) Address)[6000] == 0);
    ((UINT8 *) Address)[42] = 99;
    CHECK(Call(9, Address, 4096, 1, MAP_PRIVATE | MAP_FIXED, FD, 4096) == Address);
    CHECK(((UINT8 *) Address)[42] == 42);
    CHECK(Call(9, Address + 4096, 4096, 3, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == Address + 4096);
    CHECK(((UINT8 *) Address)[4096 + 42] == 0);
    CHECK(Call(3, FD, 0, 0, 0, 0, 0) == 0);
    CHECK(((UINT8 *) Address)[42] == 42);
    CHECK(Call(11, Address, 8192, 0, 0, 0, 0) == 0);
    UINT64 Break = Call(12, 0, 0, 0, 0, 0, 0);
    CHECK(Break > 0);
    CHECK(Call(12, Break + 8192, 0, 0, 0, 0, 0) == Break + 8192);
    ((UINT8 *) Break)[4096] = 77;
    CHECK(Call(12, Break, 0, 0, 0, 0, 0) == Break);
Done:
    FD = Call(257, -100, (UINT64) (Result == 0 ? "/linux-syscalls-PASS" : "/linux-syscalls-FAIL"), 0x41, 0644, 0, 0);
    if (FD >= 0)
    {
        Call(1, FD, (UINT64) &Result, sizeof(Result), 0, 0, 0);
        Call(3, FD, 0, 0, 0, 0, 0);
    }
    for (;;)
    {
        Call(24, 0, 0, 0, 0, 0, 0);
    }
}
