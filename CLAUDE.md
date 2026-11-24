# MFPlay

Windows Remote Play client for the PS5 (C++23, Qt 6 QML, Direct3D 12), built on
chiaki-ng's protocol library, with NVIDIA RTX enhancements. Read `README.md` for
the layout and `docs/plan.md` for the plan in progress.

## Language

- Interface text: Brazilian Portuguese.
- Code, identifiers, logs, comments, commits and docs: English.

## Build

- Never build on the developer's machine. Every build runs in GitHub Actions
  (`.github/workflows/build.yml`); the repository is public so the runners are
  free.
- After pushing, follow the run with `gh run watch` and read failures with
  `gh run view --log-failed`.
- Version and release: the `project(... VERSION x.y.z)` in `CMakeLists.txt`
  is both the app version and the release tag (`vx.y.z`). Bump it on every
  delivery; there is no build suffix.
- To test a build, download its artifact into a fresh folder and run it:
  `gh run download <run-id> --name mfplay-windows-x64 --dir <folder>`.
  Running the app on this machine is how features are checked: it is the target
  machine (RTX 4060 Laptop, NVIDIA driver, PS5 on the same network).
- Compiler: `clang-cl` for everything, as chiaki-ng's own Windows build does
  (its C code uses VLAs, which MSVC's `cl` rejects).
- vcpkg in manifest mode with the overlay triplet
  `x64-windows-static-md-release`: static libraries, dynamic CRT, release only.
- chiaki-ng is a pinned submodule in `third_party/chiaki-ng`. Don't edit it;
  adapt it from our CMake. Only its needed nested submodules are fetched
  (`scripts/init-chiaki-submodules.ps1`).
- NVIDIA's NVAPI headers are a pinned submodule in `third_party/nvapi`
  (`scripts/init-nvapi-submodule.ps1`). The driver's `nvapi64.dll` loads at
  run time; no import library is linked.
- NVIDIA and other third-party binaries (DLLs, SDKs, models) are never
  committed. The workflow downloads them from their official public sources.

## Privacy

The repository is public. Never commit or log PSN account IDs, console
registration keys, PINs, tokens or IP addresses outside the local network.

## Git

- Delivery: commit and push to `master` after each delivery of the current
  milestone, so no work is lost.
- Commits: Conventional Commits in English.
- No force-push.

## Plan and issues

- Plan and current milestone mission: `docs/plan.md`. Resume with
  `/master-plan continua o plano`.
- Issues found along the way: `docs/issues.md`.

## Design

The design file lives under `design/` on the owner's machine, outside the
repository. Its variables became the QML `Theme` singleton and its components
the QML components; the app's look is defined by those.
