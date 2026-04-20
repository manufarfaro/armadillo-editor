# md4c — vendored

Upstream: https://github.com/mity/md4c

**Pinned commit:** see `COMMIT.txt` (corresponds to release-0.5.2)

**License:** MIT (see `LICENSE`)

## What we vendor

Only the CommonMark parser — not the HTML renderer (`md4c-html.c`), not the CLI. We render markdown ourselves via our `render/` module driving QuickDraw.

**Files in this vendored copy:**
- `src/md4c.c` — the parser (~2500 lines, C89, zero deps)
- `src/md4c.h` — the public API (`md_parse`, `MD_PARSER`, block/span/text type enums)
- `COMMIT.txt` — pinned upstream commit SHA
- `LICENSE` — upstream MIT license

## Upgrading

1. Fetch the target commit from upstream (prefer a tagged release).
2. Overwrite `src/md4c.c` and `src/md4c.h`.
3. Update `COMMIT.txt` with the new SHA.
4. Run `make -f Makefile.hosttests test` — `mdparse` tests in particular will catch regressions in event sequences.
5. Commit with message `md4c: bump to <sha> for <reason>`. Never do drive-by updates.
