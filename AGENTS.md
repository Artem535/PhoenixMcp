# Repository Guidelines

## Project Structure & Module Organization

Production code is under `src/phoenix_mcp/`: `server/` contains session and
request handling, `transport/` contains stdio, Crow, and Drogon adapters,
`tool_registry/` provides typed tools, and `types/` holds protocol models.
Examples live in `examples/`; the HTTP smoke and benchmark scripts are in
`examples/create_server_http/`. Documentation is an Antora component in
`docs/modules/ROOT/pages/`; add new pages in the relevant topic directory.

`CMakeLists.txt` is handwritten and authoritative. Do not generate it from
Xmake or edit it as generated output. `xmake.lua` remains a supported secondary
build definition and mirrors the targets CMake defines.

## Build, Test, and Development Commands

- Run every repository command through `rtk`.
- `rtk run 'xmake f -m debug'` configures a C++20 debug build and resolves
  dependencies.
- `rtk run xmake` builds the library and configured targets.
- `rtk run 'xmake run create_server'` runs the stdio server example.
- `rtk run 'xmake run create_server_http'` starts the HTTP example; in another
  shell run `rtk run 'bash examples/create_server_http/smoke_test.sh'`.
- `rtk run 'xmake run using_tool_registry'` runs the typed-tool example.

The repository does not yet have a dedicated unit-test target. Treat example
and HTTP smoke checks as integration verification; add focused tests with every
behavioral change as the CTest migration lands.

## Coding Style & Naming Conventions

Use C++20 and `namespace phoenix_mcp` in public API. `pxm` is a deprecated,
temporary compatibility alias and must not be used for new public declarations.
Keep classes in `PascalCase`, functions and files in `snake_case`, member fields
in `snake_case_`, and constants in `kPascalCase`. Place declarations in `.h`
and implementations in `.cc`. Format changed C++ files with
`rtk run 'clang-format -i path/to/file.cc path/to/file.h'`; the project uses
two-space indentation and an 80-column limit. Run `clang-tidy` with the
repository `.clang-tidy` configuration through `rtk` when its compile database
is available. See the architecture code-style page for include order and header
guard rules.

## Commit & Pull Request Guidelines

Use concise, imperative commit subjects matching the existing history, e.g.
`Add Crow HTTP adapter` or `Fix session shutdown`. Keep each commit focused.
PRs should explain the protocol or lifecycle impact, list commands run, link
the issue when applicable, and include an example payload or log excerpt for
transport-facing changes. Do not commit credentials, tokens, generated build
directories, or local vcpkg paths.

## Architecture Notes

Read `docs/modules/ROOT/pages/architecture/index.adoc` before altering module
boundaries. Preserve the separation between protocol/session logic and concrete
transport adapters; process supervision and policy decisions belong to the
embedding application.
