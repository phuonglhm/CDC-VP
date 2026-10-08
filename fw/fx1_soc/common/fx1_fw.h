/*
 * FX1 SoC bare-metal support for bring-up tests and drivers: MMIO, console,
 * CLINT, hart release, traps and the VP sim-control verdict.
 *
 * All MMIO is 32-bit: FX1 APB registers (UART, CLINT, sim-control, DMA, ISP)
 * take naturally aligned word accesses.
 */
#ifndef FX1_FW_H
#define FX1_FW_H

#include <stdint.h>

#include "fx1/fx1_memory_map.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void fx1_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t*)address = value;
}
static inline uint32_t fx1_read32(uintptr_t address)
{
    return *(volatile uint32_t*)address;
}

/* ---- ordering (RVWMO) ------------------------------------------------------
 * volatile only stops the compiler; it orders nothing between harts or between
 * RAM and devices. Use these where another agent observes the order:
 *   fx1_mb()      full barrier over memory and I/O (fence iorw, iorw)
 *   fx1_wmb_io()  earlier memory writes before later device writes (fence w, o)
 *   fx1_release() earlier accesses before a later publishing store (fence rw, w)
 *   fx1_acquire() a flag/ownership read before later accesses (fence r, rw)
 */
static inline void fx1_mb(void) { __asm__ volatile("fence iorw, iorw" ::: "memory"); }
static inline void fx1_wmb_io(void) { __asm__ volatile("fence w, o" ::: "memory"); }
static inline void fx1_release(void) { __asm__ volatile("fence rw, w" ::: "memory"); }
static inline void fx1_acquire(void) { __asm__ volatile("fence r, rw" ::: "memory"); }

/* ---- buffer ownership between the CPU and a DMA master (plan C12) ----------
 * A buffer a device reads or writes changes owner twice, and each change is a
 * call, made per buffer with the direction of the device's access:
 *
 *   fx1_dma_prepare(addr, bytes, dir)   CPU -> device. After the CPU's last
 *       access, before the device is started on (or handed) the buffer. For
 *       every direction, also for a buffer the device only writes.
 *   fx1_dma_complete(addr, bytes, dir)  device -> CPU. After the device
 *       reported the buffer complete, before the CPU's first access.
 *
 * Between the two the device owns the buffer: the CPU neither reads nor writes
 * it (no speculative-safe peeking either).
 *
 * Direction is the device's access: FX1_DMA_TO_DEVICE (device reads: DMA
 * source, descriptor, ISP RAW input), FX1_DMA_FROM_DEVICE (device writes: DMA
 * destination, ISP Y/UV output), FX1_DMA_BIDIRECTIONAL (both).
 *
 * The VP models no cache: every call is a full fence and the range is unused.
 * What each call must do on silicon depends on the FX1 cache and coherency,
 * which the HAS has not fixed. For a non-coherent write-back cache the usual
 * mapping (cf. the Linux DMA API's map/unmap and sync calls) is:
 *
 *   prepare  TO_DEVICE      clean (write back) the range
 *   prepare  FROM_DEVICE    clean + invalidate, so no dirty line can be written
 *                           back over the device's data while it owns it
 *   prepare  BIDIRECTIONAL  clean + invalidate
 *   complete TO_DEVICE      nothing
 *   complete FROM_DEVICE    invalidate (lines may have been fetched
 *   complete BIDIRECTIONAL  speculatively while the device owned the buffer)
 *
 * with Zicbom cbo.clean / cbo.flush / cbo.inval (plan C8) or the HAS's own
 * maintenance. A buffer the device writes must then also start and end on a
 * cache-line boundary (line size from the HAS), or a partial line shared with
 * CPU data is corrupted. This VP does not detect a missing or misdirected
 * call: passing here says nothing about cache maintenance on silicon. Memory
 * that the CPU and a device access concurrently (the C7 atomic tests' words)
 * is outside this contract: on silicon it needs coherent or uncached memory. */
typedef enum {
    FX1_DMA_TO_DEVICE = 1,
    FX1_DMA_FROM_DEVICE = 2,
    FX1_DMA_BIDIRECTIONAL = 3,
} fx1_dma_dir_t;

static inline void fx1_dma_prepare(const volatile void* addr, uint32_t bytes, fx1_dma_dir_t dir)
{
    (void)addr;
    (void)bytes;
    (void)dir;
    fx1_mb();
}
static inline void fx1_dma_complete(const volatile void* addr, uint32_t bytes, fx1_dma_dir_t dir)
{
    (void)addr;
    (void)bytes;
    (void)dir;
    fx1_mb();
}

static inline uint32_t fx1_hartid(void)
{
    uint32_t id;
    __asm__ volatile("csrr %0, mhartid" : "=r"(id));
    return id;
}

/* ---- console (PL011-style UART at FX1_UART_BASE) ---------------------------
 * fx1_putc/puts/puthex/putdec are raw and unserialised. fx1_log* print one
 * line "[hN] text\n" under a console lock with this hart's interrupts masked
 * (saved and restored), so lines of the two harts never interleave and an
 * interrupt handler may log. A hart that faults while printing gets nested,
 * unlocked access instead of deadlocking on its own lock. */
void fx1_putc(char c);
void fx1_puts(const char* s);
void fx1_puthex(uint32_t value);
void fx1_putdec(uint32_t value);
void fx1_log(const char* text);
void fx1_log_hex(const char* text, uint32_t value);

/* ---- verdict (VP sim-control; stops the simulation) ----------------------
 * fx1_fail and the default trap handler mask interrupts, print best effort
 * (bounded wait for the console) and always write the FAIL verdict. */
void fx1_pass(void) __attribute__((noreturn));
void fx1_fail(uint32_t code) __attribute__((noreturn));
/* main() return: 0 -> pass, otherwise fail with that code. */
void fx1_exit(int code) __attribute__((noreturn));
/* Fail with `code` unless `condition` holds; prints `what`. */
void fx1_check(int condition, uint32_t code, const char* what);

/* ---- CLINT --------------------------------------------------------------- */
uint64_t fx1_mtime(void);
uint64_t fx1_rdtime(void);
void fx1_set_mtimecmp(uint32_t hart, uint64_t value);
void fx1_set_msip(uint32_t hart, uint32_t value);

/* ---- PLIC (context = FX1_PLIC_CONTEXT_HART_M(hart)) ------------------------ */
void fx1_plic_set_priority(uint32_t source, uint32_t priority);
void fx1_plic_enable(uint32_t context, uint32_t source, int on);
void fx1_plic_set_threshold(uint32_t context, uint32_t threshold);
uint32_t fx1_plic_pending(void); /* sources 0..31 */
uint32_t fx1_plic_claim(uint32_t context);
void fx1_plic_complete(uint32_t context, uint32_t source);

/* ---- VP-only test interrupt lines (sim-control) ---------------------------
 * Bit i drives PLIC source FX1_SIM_CTRL_IRQ_FIRST_SOURCE + i. Not on silicon. */
void fx1_sim_irq_set(uint32_t mask);
uint32_t fx1_sim_irq_get(void);

static inline uint32_t fx1_read_mip(void)
{
    uint32_t mip;
    __asm__ volatile("csrr %0, mip" : "=r"(mip));
    return mip;
}

/* ---- multi-hart start-up -------------------------------------------------- */
/* Set by hart 0 to let secondary harts leave the start-up WFI loop. Lives in
 * .data (0 in the image), never in .bss. */
extern volatile uint32_t fx1_secondary_release;
/* Hart 0: release every other hart into secondary_main(). Ordering: shared
 * data -> fence rw,w -> flag -> fence iorw,iorw -> MSIP write; the released
 * hart issues fence iorw,iorw after it observes the flag (crt0.S). */
void fx1_release_secondaries(void);
/* Entry of harts 1..N-1 after release; the default parks in WFI. */
void secondary_main(uint32_t hart);

/* ---- simple spin lock (AMO, shared by harts) ------------------------------- */
typedef struct { volatile uint32_t word; } fx1_lock_t;
void fx1_lock(fx1_lock_t* lock);
void fx1_unlock(fx1_lock_t* lock);

/* ---- traps ----------------------------------------------------------------- */
/* Return the mepc to resume at. Handlers run with interrupts disabled and must
 * not use floating point. */
typedef uint32_t (*fx1_trap_handler_t)(uint32_t mcause, uint32_t mepc, uint32_t mtval);
/* Install a per-hart handler; NULL restores the default (report and fail). */
void fx1_set_trap_handler(fx1_trap_handler_t handler);
uint32_t fx1_trap_dispatch(uint32_t mcause, uint32_t mepc, uint32_t mtval);

#define FX1_MCAUSE_INTERRUPT 0x80000000u
#define FX1_IRQ_M_SOFT  3u
#define FX1_IRQ_M_TIMER 7u
#define FX1_IRQ_M_EXT   11u
#define FX1_MIE_MSIE (1u << FX1_IRQ_M_SOFT)
#define FX1_MIE_MTIE (1u << FX1_IRQ_M_TIMER)
#define FX1_MIE_MEIE (1u << FX1_IRQ_M_EXT)
#define FX1_MSTATUS_MIE (1u << 3)

static inline void fx1_irq_enable(uint32_t mie_bits)
{
    __asm__ volatile("csrs mie, %0" ::"r"(mie_bits));
}
static inline void fx1_irq_disable(uint32_t mie_bits)
{
    __asm__ volatile("csrc mie, %0" ::"r"(mie_bits));
}
static inline void fx1_global_irq_enable(void)
{
    __asm__ volatile("csrs mstatus, %0" ::"r"(FX1_MSTATUS_MIE));
}
static inline void fx1_global_irq_disable(void)
{
    __asm__ volatile("csrc mstatus, %0" ::"r"(FX1_MSTATUS_MIE));
}
/* Mask this hart's interrupts and return the previous mstatus.MIE; restore
 * re-enables only if they were enabled. Use around any lock an interrupt
 * handler on the same hart may also take. */
static inline uint32_t fx1_irq_save(void)
{
    uint32_t mstatus;
    __asm__ volatile("csrrci %0, mstatus, 8" : "=r"(mstatus)::"memory");
    return mstatus & FX1_MSTATUS_MIE;
}
static inline void fx1_irq_restore(uint32_t saved)
{
    if (saved) __asm__ volatile("csrsi mstatus, 8" ::: "memory");
}
static inline void fx1_wfi(void)
{
    __asm__ volatile("wfi");
}

#ifdef __cplusplus
}
#endif

#endif /* FX1_FW_H */
