// Scripts/Validation/SecureExec/Unsafe.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#ifdef LIBRARY
int UnsafeValue(void)
{
    return 42;
}
#else
extern int UnsafeValue(void);
int        main(void)
{
    return UnsafeValue() == 42 ? 0 : 1;
}
#endif
