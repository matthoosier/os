#ifndef __REAPER_HPP__
#define __REAPER_HPP__

class Thread;
class Process;

class Reaper
{
public:
    static void Reap (Process * aProcess) __attribute__((noreturn));

    static Thread * Start ();

private:
    static void Body (void *);
};

#endif
