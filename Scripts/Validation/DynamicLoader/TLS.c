// Scripts/Validation/DynamicLoader/TLS.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

extern int Increment(int Value);
extern int OptionalValue __attribute__((weak));

__thread int TLS = 40;
int          Data;

__attribute__((constructor)) static void Initialize(void)
{
    Data = 1;
}

__attribute__((destructor)) static void Finalize(void)
{
    const char Message[] = "fini PASS\n";
    __asm__ volatile("syscall" : : "a"(1L), "D"(1L), "S"(Message), "d"(sizeof(Message) - 1) : "rcx", "r11", "memory");
}

int Value(void)
{
    if (&OptionalValue != 0)
    {
        return 0;
    }

    TLS = Increment(TLS);
    return TLS + Data;
}
