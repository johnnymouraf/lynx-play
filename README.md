# Lynx Play

A Windows Remote Play client for the PS5, built for the best picture and the
lowest input lag on NVIDIA RTX GPUs. It streams over the local network first and
over the internet when needed, and it can enhance the stream with RTX Video
Super Resolution, RTX Video HDR, DLSS Super Resolution, DLSS Frame Generation
and NVIDIA Reflex.

The streaming protocol comes from [chiaki-ng](https://github.com/streetpea/chiaki-ng)'s
library; everything around it (decoding, rendering, input, enhancements, the
interface) is written for Windows and Direct3D 12.

> Personal project, in early development. The interface is in Brazilian
> Portuguese.

## Contents

- [Status](#status)
- [Download](#download)
- [Requirements](#requirements)
- [Building](#building)
- [Project layout](#project-layout)
- [License](#license)

## Status

The plan and its progress live in [docs/plan.md](docs/plan.md).

## Download

Every push to `main` is built by GitHub Actions. Open the latest successful run
of the **Build** workflow and download the `lynx-play-windows-x64` artifact, or
use the GitHub CLI:

```powershell
gh run download --repo johnnymouraf/lynx-play --name lynx-play-windows-x64
```

Extract the zip anywhere and run `lynx-play.exe`. Logs are written to
`%LOCALAPPDATA%\LynxPlay\logs`.

## Requirements

- Windows 11 x64.
- An NVIDIA RTX GPU with a current driver for the RTX features. Streaming itself
  works on any Direct3D 12 GPU.
- A PS5 with Remote Play enabled.

## Building

The project is built in GitHub Actions (`.github/workflows/build.yml`), which is
the reference for the toolchain:

- The Visual Studio C++ build tools (Windows SDK and libraries), LLVM 20+
  `clang-cl`, CMake 3.28+ and Ninja.
- Qt 6.12 (`msvc2022_64`).
- vcpkg, in manifest mode (`vcpkg.json`).
- Python 3 with `protobuf` and `grpcio-tools`, for chiaki-ng's protocol buffers.

```powershell
git submodule update --init third_party/chiaki-ng
./scripts/init-chiaki-submodules.ps1
cmake --preset release -DCMAKE_PREFIX_PATH=<Qt dir>/msvc2022_64 -DPYTHON_EXECUTABLE=<python.exe>
cmake --build --preset release
ctest --preset release
```

## Project layout

| Path                     | What it holds                                         |
| ------------------------ | ----------------------------------------------------- |
| `src/app`                | Entry point, Qt application and QML interface         |
| `src/core`               | Logging and shared utilities                          |
| `src/render`             | Direct3D 12 device, swap chain and stream window      |
| `src/remoteplay`         | The bridge to chiaki-ng's library                     |
| `tests`                  | Unit tests (Catch2)                                   |
| `third_party/chiaki-ng`  | chiaki-ng, pinned as a submodule                      |
| `cmake`, `triplets`      | CMake helpers and the vcpkg triplet                   |

## License

[GNU Affero General Public License v3.0](LICENSE), required by chiaki-ng's
library, which is AGPL-3.0 with an OpenSSL exception. NVIDIA runtimes are
downloaded at build time under NVIDIA's own licenses and are never committed.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. NVIDIA,
RTX, DLSS and Reflex are trademarks of NVIDIA Corporation. This project is not
affiliated with or endorsed by either company.
