/* SPDX-License-Identifier: Apache-2.0
 *
 * The canonical signature block: the only thing the Spike differential run
 * compares.
 *
 * One X-macro list, included by the firmware that fills the block and by the
 * host comparator that names the fields. Neither side can add, drop or reorder
 * a word without the other following, so a mismatch is always reported against
 * the right name.
 *
 * ── what this compares, and what it does not ────────────────────────────────
 *
 * Every word here is *architecturally visible to the program*: the firmware
 * computes it with ordinary instructions and stores it. That is deliberate, and
 * it is also the limit of the method. Two models that disagree internally but
 * hide the disagreement from the program will match here. Reading each
 * simulator's private register file instead would compare more state, but it
 * would compare it through two different debug interfaces, and a difference in
 * those interfaces is indistinguishable from a difference in the models. The
 * program's own view is the one interface both are obliged to implement
 * identically.
 *
 * ── why the layout is symbol-located, not a fixed address ───────────────────
 *
 * `begin_signature` / `end_signature` are linker symbols. fesvr already dumps
 * exactly that range with `+signature=`, and the host harness finds the same
 * two symbols in the same ELF. There is no address constant to keep in step.
 *
 * ── field naming convention ─────────────────────────────────────────────────
 *
 * Fields whose value is *expected* to differ between the two models carry the
 * upstream VP++ commit that would fix them, so a diff points at a decision
 * (record D9) rather than at a mystery.
 */

#ifndef TPU_V3_RVV_SMOKE_SIG_LAYOUT_H
#define TPU_V3_RVV_SMOKE_SIG_LAYOUT_H

/* X(name, note)
 *
 * Order is the wire format. Append only; never reorder. */
#define TPU_V3_SIG_FIELDS(X)                                                   \
    /* ── run control ─────────────────────────────────────────────────── */   \
    X(magic,              "'TP3V' — proves the block was written at all")      \
    X(version,            "signature layout version")                          \
    X(status,             "0 = every firmware self-check passed, else check id")\
    X(termination,        "1 = fell off the end of main; 2 = aborted in a trap")\
    X(phase_reached,      "last corpus phase completed, for locating an abort") \
                                                                               \
    /* ── machine identity ────────────────────────────────────────────── */   \
    X(vlenb,              "VLEN/8; must be 64 for VLEN=512")                    \
    X(mhartid,            "0 on both models")                                   \
                                                                               \
    /* ── mid-vector fault and vstart resumption ──────────────────────── */   \
    X(fault_trap_count,   "traps taken by the out-of-range vector load")        \
    X(fault_mcause,       "cause of that trap")                                 \
    X(fault_mtval,        "faulting address")                                   \
    X(fault_vstart,       "element index the load stopped at")                  \
    X(fault_vl,           "vl at the trap")                                     \
    X(fault_vtype,        "vtype at the trap")                                  \
    X(fault_elem_ok,      "1 = elements below vstart were loaded correctly")     \
    X(vstart_after_clear, "vstart after an explicit csrw vstart, 0")             \
                                                                               \
    /* ── mstatus, split so one defect lands in one field ──────────────── */  \
    X(mstatus_vs_initial, "mstatus.VS after writing Initial and running vadd")  \
    X(mstatus_fs_after_fp_ls,                                                   \
      "mstatus.FS after fsw with FS=Clean; VP++ 14e7fff5 leaves it Clean")      \
    X(fp_ls_off_trapped,                                                        \
      "flw with FS=Off: 1 = illegal trap; VP++ 14e7fff5 does not trap")         \
    X(fp_ls_off_mcause,   "cause of that trap, 0 if none")                      \
    X(mstatus_other,      "mstatus with SD/FS/VS masked out")                   \
                                                                               \
    /* ── vtype / vill ────────────────────────────────────────────────── */   \
    X(vill_vtype,         "vtype after a reserved vsew; vill must be set")      \
    X(vill_vl,            "vl after a reserved vsew; must be 0")                \
    X(vill_trapped,       "must be 0: a reserved vtype sets vill, it does not trap")\
    X(vtype_normal,       "vtype after vsetvli e32,m1,ta,ma")                   \
    X(vl_normal,          "vl for AVL=16 at e32,m1")                            \
    X(vcsr,               "vxrm and vxsat")                                     \
                                                                               \
    /* ── integer vector arithmetic ───────────────────────────────────── */   \
    X(vadd_sum,           "sum of vadd.vv results")                             \
    X(vmul_sum,           "sum of vmul.vv results")                             \
    X(vwmulu_lo,          "vwmulu.vv widened result, element 0")                \
    X(vwmulu_hi,          "vwmulu.vv widened result, element 15")               \
    X(vmseq_mask,         "vmseq.vv mask bits")                                 \
    X(vcpop_count,        "vcpop.m over that mask")                             \
    X(vredsum_result,     "vredsum.vs reduction")                               \
    X(vslideup_word,      "element 4 after vslideup.vi 4")                      \
    X(vrgather_word,      "element 0 after vrgather.vv with a reversing index")  \
    X(vint_checksum,      "FNV-1a over the whole integer result buffer")        \
                                                                               \
    /* ── vector floating point ───────────────────────────────────────── */   \
    X(vfadd_rne,          "vfadd.vv of a 0.75-ulp tie, frm=RNE")                \
    X(vfadd_rtz,          "same operands, frm=RTZ")                             \
    X(vfadd_flags,        "fflags after the RTZ round; inexact only")           \
    X(vfmul_subnormal,    "vfmul.vv driving the result subnormal")              \
    X(vfmul_flags,        "fflags after it; underflow and inexact")             \
    X(vfmacc_result,      "vfmacc.vv, fused, element 0")                        \
    X(vfredusum_result,   "vfredusum.vs over 16 elements")                      \
    X(vfredosum_result,   "vfredosum.vs, ordered, over the same elements")      \
    X(vfp_checksum,       "FNV-1a over the whole FP result buffer")             \
                                                                               \
    /* ── scalar floating point (decision record D9) ──────────────────── */   \
    X(fmin_s_qnan,        "fmin.s x, qNaN; VP++ 63524fbb returns the NaN")      \
    X(fmax_s_qnan,        "fmax.s x, qNaN; VP++ 63524fbb returns the NaN")      \
    X(fmin_d_qnan_hi,     "fmin.d x, qNaN, high word; VP++ 63524fbb")           \
    X(fminmax_s_flags,    "fflags after the qNaN fmin/fmax; must stay clear")   \
    X(fmin_s_snan,        "fmin.s x, sNaN")                                     \
    X(fminmax_snan_flags, "fflags after the sNaN case; NV must be set")         \
    X(fsd_raw_lo,         "fsd of a non-canonical double, low word (c7140542)") \
    X(fsd_raw_hi,         "fsd of a non-canonical double, high word (c7140542)")\
    X(flw_nanbox_hi,      "high word of an f-register after flw; must be all ones")\
    X(fsw_raw,            "fsw of a register holding a raw non-boxed pattern")  \
    X(fdiv_inexact,       "fdiv.s 1/3, frm=RNE")                                \
    X(fsqrt_result,       "fsqrt.s 2.0")                                        \
    X(fcvt_w_s_sat,       "fcvt.w.s of +inf; must saturate to INT32_MAX")       \
    X(fcvt_flags,         "fflags after the saturating convert; NV")            \
    X(fclass_snan,        "fclass.s of a signalling NaN")                       \
    X(frm_final,          "frm at the end of the run")                          \
    X(fflags_final,       "fflags at the end of the run")                       \
                                                                               \
    /* ── instructions that must be illegal at rv32gcv ────────────────── */   \
    /* Zfh is not in the target ISA, so these are not expected diffs: both   */ \
    /* models must refuse them. A model that executes one is wrong.          */ \
    X(zfh_fadd_h_trapped, "fadd.h must raise illegal instruction")              \
    X(zfh_fadd_h_mcause,  "its cause; 2 = illegal instruction")                 \
    X(zfh_fmv_x_h_trapped,"fmv.x.h must raise illegal instruction (91777991)")  \
    X(zfh_flh_trapped,    "flh must raise illegal instruction")                 \
    X(zfh_fsh_trapped,    "fsh must raise illegal instruction (c7140542)")      \
                                                                               \
    /* ── RV32 index EEW=64 (audit finding F12) ───────────────────────── */   \
    /* v-spec 1.0 §18.2: the V extension does not support EEW=64 for index  */ \
    /* values when XLEN=32, and §7.3 requires an illegal-instruction        */ \
    /* exception for an unsupported offset EEW.                             */ \
    X(eew64_trapped,      "vluxei64.v on RV32 must raise illegal instruction")  \
    X(eew64_mcause,       "its cause")                                          \
                                                                               \
    /* ── whole-memory result checksum ────────────────────────────────── */   \
    X(mem_checksum,       "FNV-1a over every result buffer in one pass")        \
    X(trap_total,         "traps taken across the entire run")

/* Word indices, generated from the same list for whichever language is asking.
 *
 * The startup code writes two of these words on the abort path, so the indices
 * have to exist in assembly too. Emitting them from the same X-macro rather
 * than hand-copying two numbers is the whole reason this file is shared. */
#ifdef __ASSEMBLER__

    .set tpu_v3_sig_next, 0
#define TPU_V3_SIG_ASM_SET(name, note)                                         \
    .set TPU_V3_SIG_##name, tpu_v3_sig_next;                                   \
    .set tpu_v3_sig_next, tpu_v3_sig_next + 1;
    TPU_V3_SIG_FIELDS(TPU_V3_SIG_ASM_SET)
#undef TPU_V3_SIG_ASM_SET
    .set TPU_V3_SIG_WORD_COUNT, tpu_v3_sig_next

#else

enum tpu_v3_sig_index {
#define TPU_V3_SIG_ENUM(name, note) TPU_V3_SIG_##name,
    TPU_V3_SIG_FIELDS(TPU_V3_SIG_ENUM)
#undef TPU_V3_SIG_ENUM
    TPU_V3_SIG_WORD_COUNT
};

#endif /* __ASSEMBLER__ */

#define TPU_V3_SIG_MAGIC 0x54503356u /* 'TP3V' */
#define TPU_V3_SIG_VERSION 1u

/* Termination reasons. */
#define TPU_V3_TERM_NORMAL 1u
#define TPU_V3_TERM_TRAP_ABORT 2u

/* What the trap handler tells the startup code to do next. Same encoding as
 * `sim_exit.h`, kept here so this image does not depend on that header. */
#define SIG_TRAP_ACTION_ABORT 0u
#define SIG_TRAP_ACTION_RESUME 1u
#define SIG_TRAP_ACTION_SKIP 2u

#endif /* TPU_V3_RVV_SMOKE_SIG_LAYOUT_H */
