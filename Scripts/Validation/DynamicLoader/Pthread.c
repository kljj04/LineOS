#include <pthread.h>
#include <errno.h>
#include <unistd.h>
__thread int TLSValue = 9;

static void *Run(void *Arg)
{
    errno = 19;
    if (TLSValue++ != 9)
        return 0;
    return Arg;
}
int main(void)
{
    pthread_t Thread;
    void     *Value = 0;
    int       Arg;
    errno = 37;
    if (pthread_create(&Thread, 0, Run, &Arg) || pthread_join(Thread, &Value) || Value != &Arg || errno != 37 || TLSValue != 9)
        return 1;
    write(1, "pthread loader PASS\n", 20);
    return 0;
}
