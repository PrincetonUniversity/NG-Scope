#ifndef __CPU_H__
#define __CPU_H__

#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))
#define pin_thread(...)  (_pin_thread(NUMARGS(__VA_ARGS__), __VA_ARGS__))

int _pin_thread(int num_cpus, ...);

#endif