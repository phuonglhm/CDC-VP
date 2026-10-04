# Data package (split archive)

`sauria_npu_data.tar.gz.partNN` are the pieces of one archive, `sauria_npu_data.tar.gz`, cut below 100 MB so that
the repository can be pushed to a git host without large-file support. `SHA256SUMS` holds the sha256 of the joined
archive.

From the repository root:

```bash
bash tools/unpack_data.sh                 # joins, checks and extracts to ./sauria_npu_data
export FE_WORK=$PWD/sauria_npu_data
```

The script refuses to continue when the checksum of the joined parts or of any extracted file differs. The
extracted directory (about 810 MB) is listed in `.gitignore`. Its content is described in its own `README.md` and
in `RELEASE_NOTES.md`, section 1.

By hand: `cat data/sauria_npu_data.tar.gz.part* > sauria_npu_data.tar.gz`, compare `sha256sum sauria_npu_data.tar.gz`
with `data/SHA256SUMS`, then `tar xzf sauria_npu_data.tar.gz`.
