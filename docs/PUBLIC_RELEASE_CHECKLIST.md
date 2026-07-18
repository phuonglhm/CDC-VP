# Public release checklist

This checklist is a release gate, not a substitute for legal review.

Run the static gate from the repository root:

```bash
tools/check_public_release.sh
```

During development, `--allow-dirty` skips only the clean-working-tree check;
it does not suppress license or provenance blockers.

## Required before publishing

- [ ] Confirm the organization owns or is authorized to release every CDC-VP
      contribution under Apache-2.0, including employee contributions.
- [ ] Ensure `git status --short` contains no intended-but-untracked license,
      notice, provenance, source, or ABI files.
- [ ] Review `LICENSE`, `NOTICE`, `THIRD_PARTY.md`, and every file under
      `licenses/`.
- [ ] Resolve every `BLOCKED` row in
      `components/isp_tlm/ASSET_PROVENANCE.md`.
- [ ] Confirm the VPU TLM 3.0 rights holder and complete its MIT copyright
      notice as described in `components/vpu_tlm3.0/PROVENANCE.md`.
- [ ] Confirm no private source paths, credentials, internal repository URLs,
      employee-only identifiers, or confidential history are present in the
      release branch.
- [ ] Confirm all tracked third-party binaries and source subtrees are listed
      in `THIRD_PARTY.md` with their licenses and provenance.
- [ ] Build the public configuration with
      `CDC_ENABLE_SAURIA_NPU_V4=OFF`.
- [ ] Confirm the public artifacts contain neither the private SAURIA SystemC
      implementation nor an NPU-enabled platform binary.
- [ ] Run the full public build and CTest suite.

## SAURIA/NPU boundary

- Public CDC-VP may contain the project-owned Apache-2.0 TLM adapter, register
  ABI, firmware example, tests, upstream license, and attribution.
- `SAURIA_NPU_ROOT` is an authorized internal build input and must never be
  committed.
- Publishing an NPU-enabled binary requires separate approval from the rights
  owner of the external SystemC implementation.
- Keep `licenses/SAURIA.SHL-2.1` and
  `licenses/SAURIA.PROVENANCE.md` together.

## Release evidence

Record the release commit, build configuration, compiler versions, test
results, binary hashes, and the person approving the release. Do not release a
working tree marked `-dirty`.
