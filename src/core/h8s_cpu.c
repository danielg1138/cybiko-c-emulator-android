/* Cybiko H8S/2000 CPU - stub
 *
 * Minimal implementation to satisfy linker for peripheral modules.
 * Full CPU emulation will be implemented later.
 */
#include "core/h8s_cpu.h"

/* Stub CPU struct - will be fully defined when CPU module is implemented */
struct h8s_cpu {
    int placeholder;
};

void h8s_cpu_request_interrupt(h8s_cpu_t *cpu, int vector) {
    (void)cpu;
    (void)vector;
    /* TODO: queue interrupt for CPU processing */
}
