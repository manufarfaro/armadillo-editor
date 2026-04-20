# Unity — vendored

Upstream: https://github.com/ThrowTheSwitch/Unity

**Pinned version:** v2.6.1 (tag)

**License:** MIT (see `LICENSE.txt`)

**Files in this vendored copy:**
- `unity.c` — the implementation
- `unity.h` — the public API macros (`TEST_ASSERT_*`)
- `unity_internals.h` — internal macros
- `LICENSE.txt` — upstream license

## Upgrading

1. Download a new tagged release from GitHub.
2. Copy `src/unity.c`, `src/unity.h`, `src/unity_internals.h`, `LICENSE.txt` over the files here.
3. Update the "Pinned version" line above.
4. Run `make -f Makefile.hosttests test` and verify no regressions.
5. Commit with message `unity: bump to vX.Y.Z for <reason>`.
