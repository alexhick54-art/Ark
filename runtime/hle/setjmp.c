// Ark: setjmp / longjmp for translated code (Legends: Arceus's Lua raises errors with longjmp).
//
// A guest longjmp ends with `br x30` to the instruction after the setjmp call, inside a translated function. That
// can't be dispatched to (it's no function entry), and the host frames in between have to go too. So:
// - every call site of setjmp in the translated code (tools/lifter/gen.py) first takes a host setjmp in its own
//   frame and registers it here under the guest jmp_buf's address, then calls the guest setjmp as usual;
// - the guest longjmp is replaced (tools/hle_symbols.txt) by hle_longjmp below: restore the guest registers the
//   guest setjmp saved (musl's AArch64 layout: x19-x30, sp at +0x68, d8-d15 from +0x70), set x0, and host-longjmp
//   to the checkpoint: the translated function carries on after its setjmp call with setjmp's second return value.
#include <setjmp.h>
#include <string.h>

#include "../hle.h"

typedef struct {
    uint64_t env;  // guest jmp_buf
    jmp_buf *hb;   // host checkpoint (on the host stack of a translated function's frame)
} JmpReg;

#define MAX_JMP 256
static __thread JmpReg regs[MAX_JMP];
static __thread int nregs;

void rt_setjmp_register(uint64_t env, jmp_buf *hb) {
    // Checkpoints deeper in the host stack than this one (lower addresses: the stack grows down) belong to frames
    // that have returned since: drop them. A re-registration of the same guest buffer replaces the old one.
    while (nregs && (uintptr_t)regs[nregs - 1].hb <= (uintptr_t)hb) nregs--;
    for (int i = nregs - 1; i >= 0; i--)
        if (regs[i].env == env) { memmove(&regs[i], &regs[i + 1], (nregs - i - 1) * sizeof *regs); nregs--; break; }
    if (nregs == MAX_JMP) FATAL("setjmp: more than %d live checkpoints\n", MAX_JMP);
    regs[nregs++] = (JmpReg){env, hb};
}

static void do_longjmp(Cpu *c) {
    uint64_t env = c->x[0];
    int i = nregs - 1;
    while (i >= 0 && regs[i].env != env) i--;
    if (i < 0) FATAL("longjmp to a guest jmp_buf %llx with no live setjmp checkpoint\n", (unsigned long long)env);
    const uint64_t *e = (const uint64_t *)(uintptr_t)env;
    for (int r = 19; r <= 30; r++) c->x[r] = e[r - 19];
    c->sp = e[13];
    for (int r = 8; r <= 15; r++) c->v[r] = (__uint128_t)e[14 + (r - 8)];
    c->x[0] = c->x[1] ? c->x[1] : 1;
    jmp_buf *hb = regs[i].hb;
    nregs = i + 1;  // (checkpoints made below the target are gone with their frames)
    longjmp(*hb, 1);
}

void hle_longjmp(Cpu *c) { do_longjmp(c); }
void hle__longjmp(Cpu *c) { do_longjmp(c); }
