#include <sys/process.h>

int main (int argc, char *argv[]) {

    int pid;
    int my_pid = GetPid();

    pid = Spawn("echo");
    pid = Spawn("uio");
    pid = Spawn("pl011");
    pid = Spawn("crasher");

    my_pid = my_pid;
    pid = pid;

    return 0;
}
