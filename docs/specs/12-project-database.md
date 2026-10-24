# 12 · Project Database

One project per ROM (keyed by SHA-1), stored as a SQLite file `<name>.genproj` (D-009), plus an optional
text export for version control.

## Tables (initial schema)

| Table | Key columns |
|-------|-------------|
| `rom` | sha1, crc32, size, header fields, region, revision label |
| `symbols` | id, address_space (rom/ram68k/ramz80/io), address, name, kind (function/label/data/ram/struct_field), source (user/llm/reference/hardware/auto), visible, created_at |
| `symbol_history` | symbol_id, old_name, new_name, source, timestamp |
| `comments` | address_space, address, kind (line/block/function), text |
| `regions` | start, end, kind (code68k/codez80/data/unknown), data_type, struct_id, confidence, source |
| `structs` / `struct_fields` | struct name, size; field offset, size, name, type |
| `functions` | entry, end, name_symbol_id, flags (leaf, every_frame, decompressor:<fmt>) |
| `xrefs` | from_addr, to_addr, kind (call/jump/branch/read/write/pointer), observed (bool) |
| `jump_tables` | addr, entry_size, count, base, resolved (bool) |
| `coverage_layers` | id, name, kind (session/watch/merged), frame_start, frame_end, created_at |
| `coverage_data` | layer_id, address_space, blob (compressed per-byte flags + counts) |
| `llm_results` | id, function_entry, model, prompt_version, context_hash, response_json, usage, cost, created_at |
| `llm_proposals` | result_id, target, proposed_name, confidence, status (pending/accepted/rejected/edited) |
| `bookmarks` | address, name, color, note |
| `settings` | key, value (per-project config: thresholds, palettes, blind mode) |
| `routine_roles` | function_entry, role, format (e.g. `decompressor:nemesis`, `sprite_builder`) |
| `subsystems` | id, name, root_entries, seed_layer_id, status (spec 15) |
| `subsystem_members` | subsystem_id, kind (function/data/ram/vram/sound_id/irq_handler), start, end, confidence, source |
| `contracts` | subject (function entry or subsystem id), contract_json, cross_check_status, accepted |

## Workspace (multiple projects)

Splicing needs a donor and a target project open at the same time. A **workspace** file
(`<name>.genws`, JSON) lists the projects it contains (by path and ROM SHA-1) and the splice plans between
them (spec 15). Cross-project references are stored as `(project sha1, address space, address)` and never as
raw pointers into another database.

- Migrations are versioned (`schema_version` table); older projects upgrade on open, with a backup copy made first.
- Writes from the UI are transactional and immediate (no "unsaved project" state, except for an explicit undo stack for renames/types).

## Text export

`genproj export --text` writes a deterministic, sorted directory of YAML/CSV files (symbols, comments,
regions, structs, functions) suitable for git diffs and merges. `genproj import --text` reconstructs a
database. Coverage blobs and LLM raw responses are excluded from the text export by default.
