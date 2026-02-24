#ifndef CYBIKO_H8S_CPU_H
#define CYBIKO_H8S_CPU_H
#include "types.h"

/* Forward declaration of the CPU struct.
 * The full definition will be provided when the CPU module is implemented.
 * Peripheral modules (timers, etc.) only need the pointer and this function. */
typedef struct h8s_cpu h8s_cpu_t;

void h8s_cpu_request_interrupt(h8s_cpu_t *cpu, int vector);

#endif
