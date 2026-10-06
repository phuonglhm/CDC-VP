# Mutation checks (M3–M5)

A mutation is one deliberate rule change in a scratch copy of the sources.
It is **killed** if at least one test fails. A survivor either reveals a test
gap, which was fixed and the mutation re-run, or is **equivalent**: it cannot
change any observable behaviour of the model, for the reason given. The
scratch harness copies sources by checksum with a fresh mtime, so every
mutant is rebuilt (M3-R6).

| Area | Mutants | Killed | Equivalent | Gaps found and fixed |
|---|---|---|---|---|
| Image blocks (M3) | EE gain order, CNF inclusive thresholds, 2DNR lower median, border policies | all | — | the pipeline vectors missed the first three: block-level vectors added (M3-R2) |
| Statistics accumulation and taps (M3) | 17 | 17 | 0 | no vector had WB or GTM enabled, or a grid smaller than the frame (M3-R3) |
| Statistics publication, readout, resets (M3) | 24 | 21 | 3 | a test value hid a missing UE mask (M3-R6) |
| Continuous operation, cross-frame state (M4) | 6 | 4 | 0 | DG had no G-CONT delta (M4-R3); 2 not observable at burst boundaries, covered by `test_stats` |
| Block busy bits (M5) | 3 | 3 | 0 | — |
| Reference driver (M5) | 15 | 10 | 5 | the first driver test missed wrong-buffer recovery, the DONE acknowledge, the queue limit and the start-up order (M5-R4) |

The statistics survivors are equivalent for these reasons:
- the global count mask: `register_file::sw_read` masks every read to the
  field's defined bits;
- the OE mask: a 32-bit shift drops the upper bits;
- "AWB limit read live": the configuration is snapshotted right after the
  SOF, which is the same as sampling at SOF.

The driver survivors are equivalent on this model:
- not closing the CCM gate before writing a set: it differs only if a SOF
  falls inside the update;
- the early busy check of the zone reader: its final check repeats it;
- VALID before FREE: the output reaches the ODMA after it has armed;
- the retry of the global readers: it needs a publication racing the read;
- rewriting an unchanged LSC mesh size: only a change invalidates profiles.

One driver mutant was reformulated after its first form turned out to be
equivalent by construction: "first instead of last released buffer". Its
corrected form is killed. It is not counted twice above.
