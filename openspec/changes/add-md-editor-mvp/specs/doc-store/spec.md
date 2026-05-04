# Delta Spec: doc-store

## ADDED Requirements


### Requirement: Opaque handle

The `Doc` type SHALL be declared as an opaque struct in `src/doc.h`. Callers SHALL NOT access fields directly; all manipulation goes through the module's public API.

#### Scenario: Opaque type compiles
- GIVEN a caller includes only `src/doc.h`
- WHEN the caller attempts to access `Doc::text_buf` directly
- THEN compilation fails (the struct is incomplete outside the implementation file)

### Requirement: Lifecycle

The module SHALL provide `doc_new()` returning a fresh `Doc*` with empty text and `dirty=false`, and `doc_free(Doc*)` releasing all allocations. `doc_new()` SHALL return `NULL` if allocation fails. `doc_free(NULL)` SHALL be a no-op.

#### Scenario: Fresh Doc has empty state
- GIVEN `doc_new()` returns a non-NULL pointer
- WHEN the caller reads its initial state
- THEN `doc_text()` returns a pointer (possibly to an empty buffer) with `*out_len == 0`
- AND `doc_is_dirty()` returns 0

#### Scenario: Double-free is safe via NULL
- GIVEN `doc_free(doc)` has been called and `doc` has been set to NULL
- WHEN `doc_free(NULL)` is called subsequently
- THEN no crash occurs

### Requirement: Text buffer access

The module SHALL provide:

```c
const char*  doc_text(const Doc*, unsigned short* out_len);
void         doc_set_text(Doc*, const char* bytes, unsigned short len);
```

`doc_set_text` SHALL copy `len` bytes from `bytes` into an internal buffer, overwriting previous contents, and SHALL set the dirty flag to 1. `doc_text` SHALL return a pointer to the current buffer and write the current length to `*out_len`. The returned pointer SHALL remain valid until the next `doc_set_text` or `doc_free`.

#### Scenario: Set and read back text
- GIVEN `doc_set_text(d, "Hello", 5)` has been called
- WHEN the caller invokes `doc_text(d, &len)`
- THEN the returned pointer points to 5 bytes matching "Hello"
- AND `len == 5`

#### Scenario: Overwrite text
- GIVEN `doc_set_text(d, "First", 5)` followed by `doc_set_text(d, "Second", 6)`
- WHEN `doc_text(d, &len)` is called
- THEN the returned bytes match "Second" and `len == 6`

### Requirement: Dirty flag

The module SHALL track a boolean dirty flag indicating unsaved changes. `doc_mark_dirty(Doc*)` sets it; `doc_mark_clean(Doc*)` clears it; `doc_is_dirty(const Doc*)` returns 0 or 1. `doc_set_text` SHALL implicitly mark the doc dirty.

#### Scenario: Set text marks dirty
- GIVEN a freshly created Doc with `doc_is_dirty() == 0`
- WHEN `doc_set_text` is called with any content
- THEN `doc_is_dirty()` returns 1

#### Scenario: Save path clears dirty
- GIVEN a dirty Doc
- WHEN the caller invokes `doc_mark_clean(doc)` after a successful save
- THEN `doc_is_dirty()` returns 0

### Requirement: Filename association

The module SHALL provide accessors to associate a filesystem location with the Doc:

```c
void doc_set_filename(Doc*, const char* path, unsigned char path_len);
int  doc_has_filename(const Doc*);
const char* doc_filename(const Doc*, unsigned char* out_len);
```

Filename storage is an opaque byte sequence — it does NOT include `FSSpec` in the public header. The `file-io` module converts between `FSSpec` and this byte representation internally (for example, Pascal-string-ish or a small serialized form).

#### Scenario: Fresh Doc has no filename
- GIVEN `doc_new()` returns a fresh Doc
- WHEN `doc_has_filename` is called
- THEN the return value is 0

#### Scenario: Set and read filename
- GIVEN `doc_set_filename(d, "notes.md", 8)` has been called
- WHEN `doc_filename(d, &len)` is called
- THEN the returned bytes match "notes.md" and `len == 8`
- AND `doc_has_filename(d)` returns 1

### Requirement: No Toolbox dependencies

The module SHALL compile cleanly with `-std=c89` using host `cc` without any Toolbox or System 7 headers. No `#include <Handle.h>`, `<FSSpec.h>`, `<TextEdit.h>`, etc.

#### Scenario: Host-buildable
- GIVEN the file `src/doc.c` and a minimal `int main() { return 0; }` harness
- WHEN compiled with `cc -std=c89 -Wall -Werror -Isrc src/doc.c main.c`
- THEN compilation succeeds with no warnings or errors

### Requirement: Bounded document size

The module SHALL enforce an upper bound on text length via `kMaxDocBytes` (default 32767 in MVP, configurable via `-D`). `doc_set_text` with `len > kMaxDocBytes` SHALL reject the call and leave the Doc's state unchanged; the caller's contract is to enforce the limit upstream (for example, in `file_io_open`).

#### Scenario: Over-limit set_text is rejected
- GIVEN `kMaxDocBytes == 32767`
- WHEN the caller invokes `doc_set_text(d, big_bytes, 50000)`
- THEN the call returns (either via no-op or a distinguished error path — module's choice)
- AND the Doc's state is unchanged from before the call
