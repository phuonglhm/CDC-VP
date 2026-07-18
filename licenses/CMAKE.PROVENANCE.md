# CMake binary installer provenance

- File: `cmake-3.21.7-linux-x86_64.sh`
- Product: CMake 3.21.7 Linux x86-64 self-extracting installer
- Upstream: <https://github.com/Kitware/CMake/releases/tag/v3.21.7>
- Rights notice: Copyright 2000-2021 Kitware, Inc. and Contributors
- License: BSD-3-Clause
- SHA-256:
  `47bfc0d1c81051c83429240195d0b747107db99c4153ba89582ca370986cce9d`

The installer is an unmodified third-party development tool and is not linked
into CDC-VP or any platform binary. Its interactive preamble embeds and
displays the same copyright and BSD-3-Clause terms reproduced in
`licenses/CMAKE.BSD-3-Clause`.

If the installer is replaced, update the version, upstream URL, embedded
license text, and SHA-256 together. Removing the installer and documenting
CMake as a host prerequisite is preferable for a smaller public source tree.
