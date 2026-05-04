# Delta Spec: file-io

## ADDED Requirements


### Requirement: Error enum

The file-io module SHALL surface failures via the following error enum:

```c
typedef enum {
    kFileIoOk       =  0,
    kFileIoErrOpen  = -1,   /* file not found or couldn't open data fork */
    kFileIoErrStat  = -2,   /* couldn't get file metadata (EOF, etc.) */
    kFileIoErrTooBig= -3,   /* file exceeds kMaxDocBytes */
    kFileIoErrOOM   = -4,   /* NewHandle / doc_new failed */
    kFileIoErrRead  = -5,   /* FSRead returned an error */
    kFileIoErrWrite = -6,   /* FSWrite returned an error */
    kFileIoErrClose = -7,   /* FSClose / cleanup failed */
    kFileIoErrCancel= -8,   /* user canceled Standard File dialog */
} FileIoError;
```

#### Scenario: Error codes are stable
- GIVEN a call to `file_io_open` that fails
- WHEN the error code is compared to a named enum
- THEN the comparison uses the named symbol, not a magic number

### Requirement: Open with Standard File dialog

The file-io module SHALL expose an interactive open entry point with this signature:

```c
int file_io_open_interactive(Doc** out_doc, const MacSyscalls*);
```

This function SHALL:

1. Call `MacSyscalls.standard_get_file` to present the standard Open dialog, filtered for `'TEXT'` files (typeList containing `'TEXT'`).
2. If the user cancels, return `kFileIoErrCancel` with `*out_doc` unchanged.
3. If the user selects a file, call `file_io_open` with the resulting `FSSpec` and propagate its return value.

#### Scenario: User cancels Open
- GIVEN a `MacSyscalls` whose `standard_get_file` sets `sfGood = false`
- WHEN `file_io_open_interactive` is called
- THEN it returns `kFileIoErrCancel`
- AND `*out_doc` is not modified

#### Scenario: User selects a file
- GIVEN a `MacSyscalls` whose `standard_get_file` returns a valid `FSSpec` for a 10-byte text file
- WHEN `file_io_open_interactive` is called
- THEN a new `Doc*` is constructed with 10 bytes of text
- AND the function returns `kFileIoOk`

### Requirement: Open by path

The file-io module SHALL expose a path-based open entry point with this signature:

```c
int file_io_open(const void* fsspec_opaque, Doc** out_doc, const MacSyscalls*);
```

`fsspec_opaque` is a `const FSSpec*` cast to `void*` — the vendor-free-header convention (no `FSSpec` in the header). The function SHALL:

1. Open the file's data fork read-only via `MacSyscalls.fsp_open_df`.
2. Query size via `MacSyscalls.get_eof`.
3. If size > `kMaxDocBytes`, close and return `kFileIoErrTooBig`.
4. Allocate a `Handle` of `size` bytes via `MacSyscalls.new_handle`.
5. `HLock` the handle and `FSRead` the bytes into it.
6. Construct a `Doc*` via `doc_new()` and copy the bytes via `doc_set_text` (the Doc owns its own storage; the temporary `Handle` is disposed after).
7. Record the FSSpec on the Doc via `doc_set_fsspec(doc, fsspec_opaque)` so a subsequent `file_io_save` can write back to the same location without re-prompting.
8. Record the filename on the Doc via `doc_set_filename`, copied from the FSSpec's Pascal-string `name` field at byte offset 6 (length byte at [6], characters at [7..]). Reading the layout directly avoids depending on `<Files.h>` in this host-buildable file.
9. Close the file reference.
10. On any failure, release all intermediate resources via goto-cleanup and return the appropriate error code.

#### Scenario: Successful open
- GIVEN a valid markdown file on disk
- WHEN `file_io_open` is called
- THEN `kFileIoOk` is returned
- AND `*out_doc` points to a Doc with the file's bytes of text
- AND `doc_has_fsspec(*out_doc)` returns 1
- AND `doc_has_filename(*out_doc)` returns 1 when the FSSpec carries a non-empty Pascal-string name

#### Scenario: File exceeds size limit
- GIVEN a 40000-byte file on disk (> 32767)
- WHEN `file_io_open` is called
- THEN `kFileIoErrTooBig` is returned
- AND `*out_doc` is not modified

#### Scenario: Read error mid-sequence
- GIVEN a file that opens successfully but `FSRead` returns `ioErr`
- WHEN `file_io_open` is called
- THEN `kFileIoErrRead` is returned
- AND the file reference is closed (not leaked)
- AND the intermediate Handle is disposed

### Requirement: Save with Standard File dialog (Save As)

The file-io module SHALL expose an interactive save-as entry point with this signature:

```c
int file_io_save_as(Doc*, const MacSyscalls*);
```

This function SHALL:

1. Call `MacSyscalls.standard_put_file` to present the standard Save dialog.
2. On cancel, return `kFileIoErrCancel`.
3. On confirm, attempt `MacSyscalls.fsp_create` with creator `'Arma'` and type `'TEXT'`. If the file already exists (`dupFNErr`), proceed to step 4 anyway — the file-write sequence will overwrite it.
4. Record the FSSpec on the Doc via `doc_set_fsspec(d, fsspec)` and the leaf name via `doc_set_filename(d, fsspec.name + 1, fsspec.name[0])`.
5. Call `file_io_save` with the now-stored FSSpec and propagate its return value.

#### Scenario: Save As from scratch
- GIVEN a Doc with no FSSpec and text "Hello"
- WHEN `file_io_save_as` is called and the user picks "newfile.md"
- THEN `kFileIoOk` is returned
- AND the file "newfile.md" exists on disk containing "Hello"
- AND `doc_has_fsspec(d)` now returns 1
- AND `doc_has_filename(d)` now returns 1

### Requirement: Save to existing filename

The file-io module SHALL expose a save-to-cached-filename entry point with this signature:

```c
int file_io_save(Doc*, const MacSyscalls*);
```

This function SHALL:

1. Read the Doc's stored FSSpec via `doc_fsspec`; if the Doc has no FSSpec (`doc_has_fsspec == 0`), return `kFileIoErrOpen` with no I/O performed (caller should use `file_io_save_as` first).
2. Open the file's data fork read/write via `MacSyscalls.fsp_open_df` with permission `fsRdWrPerm` (3).
3. Truncate to 0 via `MacSyscalls.set_eof(ref, 0)`.
4. Write the Doc's text bytes via `MacSyscalls.fs_write` (the full count is passed in `io_count`).
5. Close the file reference via `MacSyscalls.fs_close`.
6. Call `doc_mark_clean(d)` on success.

The file's `TEXT` type and creator code are preserved across saves. On a new file (created by `file_io_save_as`'s underlying `fsp_create`), the creator code is set to `'Arma'` and type to `'TEXT'`.

#### Scenario: Save preserves byte-for-byte content
- GIVEN a Doc with text bytes `[0x23, 0x20, 0x48, 0x69]` ("# Hi") and a stored FSSpec
- WHEN `file_io_save` is called
- THEN the file on disk contains exactly those 4 bytes, no line-ending conversion, no BOM, no trailing newline

#### Scenario: Save clears dirty flag
- GIVEN a dirty Doc with a stored FSSpec
- WHEN `file_io_save` succeeds
- THEN `doc_is_dirty(d)` returns 0

#### Scenario: Save without FSSpec fails cleanly
- GIVEN a Doc with `doc_has_fsspec == 0`
- WHEN `file_io_save` is called
- THEN `kFileIoErrOpen` is returned
- AND no file I/O is attempted

### Requirement: Resource cleanup on all paths

Every `file_io_*` function SHALL use the goto-cleanup pattern: on any failure path, all opened file references are closed and all allocated handles disposed before return. No leaks on error paths.

#### Scenario: Every cleanup path runs
- GIVEN a function call that fails at step N of K
- WHEN the function returns an error
- THEN exactly the resources acquired in steps 1..N-1 are released

### Requirement: Line-ending policy

The MVP SHALL NOT perform any line-ending conversion. Files are read and written byte-for-byte. If a file contains CR line endings (classic Mac convention), they are stored as CR in the Doc's buffer and surface to the source pane as-is. If it contains LF (Unix) or CRLF (Windows), same. The source pane's text engine handles display.

#### Scenario: CR line endings preserved
- GIVEN a file on disk with bytes `[0x41, 0x0D, 0x42]` ("A", CR, "B")
- WHEN opened via `file_io_open` then saved again via `file_io_save`
- THEN the file on disk still contains exactly `[0x41, 0x0D, 0x42]`

### Requirement: Host testability (data-path parts)

The non-Standard-File portions of `file_io` SHALL be host-testable via `FakeSyscalls`. The fake provides controllable `open_df` / `get_eof` / `new_handle` / `fs_read` / `fs_write` / `set_eof` / `fs_close` implementations that let tests verify error handling, cleanup ordering, and size-limit enforcement without a real File Manager.

The Standard File dialog parts (`file_io_open_interactive`, `file_io_save_as`) are NOT host-testable in MVP — their dialogs are modal and require a running Toolbox. They are verified via the on-device smoke test instead.

#### Scenario: Size-limit enforcement tested without a real file
- GIVEN a `FakeSyscalls` where `get_eof` returns 50000
- WHEN `file_io_open(&spec, &doc, (const MacSyscalls*)&fake)` is called
- THEN the return value is `kFileIoErrTooBig`
- AND `fake.fs_read_calls == 0` (no read attempted)
- AND `fake.dispose_handle_calls` matches `fake.new_handle_calls` (no leaks)
