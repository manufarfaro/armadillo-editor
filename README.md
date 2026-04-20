# Armadillo Editor

A System 7 markdown editor for 68030+ classic Macintosh. Cross-compiled with Retro68.

Current status: **pre-MVP** — this repo holds the PRD, OpenSpec change, capability specs, and implementation plans. Source code is being implemented per `openspec/changes/add-md-editor-mvp/plan-1-host-pipeline.md`.

## Where things live

- `PRD.md` — product requirements
- `openspec/` — change proposals, designs, authoritative capability specs
- `src/`, `src_pane/`, `render/`, `mdparse/`, `scanner/` — source modules (created during plan execution)
- `third_party/md4c/` — vendored markdown parser
- `test/` — host unit-test support (Unity, fakes, recorder)

Expanded README lands in Plan 2.
