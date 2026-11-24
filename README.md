# MFPlay

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
- [Playing a test stream](#playing-a-test-stream)
- [Requirements](#requirements)
- [Building](#building)
- [Project layout](#project-layout)
- [License](#license)

## Status

The plan and its progress live in [docs/plan.md](docs/plan.md).

## Download

Every green build of `main` publishes a versioned release
(`v0.1.0-build.N`) with the portable package,
`mfplay-windows-x64.zip`: extract it anywhere and run `mfplay.exe`.
There is no installer.

Download the newest release with the GitHub CLI, or from the Releases page:

```powershell
gh release download --repo johnnymouraf/mfplay --pattern "mfplay-windows-x64.zip"
```

Logs are written to `%LOCALAPPDATA%\MFPlay\logs`. The newest twelve
releases are kept; older ones are removed by the build. The saved and paired consoles live in
`%APPDATA%\MFPlay\consoles.json`, with their keys and the PSN account ID
encrypted for the current Windows user (DPAPI), and the video settings in
`%APPDATA%\MFPlay\settings.json`.

The first run asks Windows Firewall to let MFPlay use the network: allow it,
or the consoles' answers to the network search never reach the app.

## Playing a test stream

To check the video path without a console, play an H.264 or H.265 elementary
stream file (`.h264`, `.264`, `.h265`, `.265`, `.hevc`) in a loop:

```powershell
.\mfplay.exe --play-file test1080p60.h265 --fps 60
```

It opens only the stream window, decodes on the GPU and renders exactly as a
PS5 stream does, and logs the decoded and presented frame rates. `--fps`
defaults to 60. FFmpeg makes a suitable file (no B-frames, as a console
sends):

```powershell
ffmpeg -f lavfi -i testsrc2=size=1920x1080:rate=60 -t 20 `
  -vf "scale=out_color_matrix=bt709:out_range=tv,format=yuv420p" `
  -c:v libx265 -x265-params "keyint=60:bframes=0" test1080p60.h265
```

An HDR10 file (10-bit P010, BT.2020/PQ) follows the same path and is
tone-mapped on an SDR display:

```powershell
ffmpeg -f lavfi -i testsrc2=size=1920x1080:rate=60 -t 20 `
  -vf "zscale=min=bt709:tin=bt709:pin=bt709:m=bt2020nc:t=smpte2084:p=bt2020:r=limited:npl=1000,format=yuv420p10le" `
  -c:v libx265 -x265-params "keyint=60:bframes=0:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc" `
  test-hdr10.h265
```

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
- Qt 6.10 (`msvc2022_64`); newer Qt releases use a repository layout the CI's
  installer (aqtinstall) can't read yet.
- vcpkg, in manifest mode (`vcpkg.json`).
- Python 3 with `protobuf` and `grpcio-tools`, for chiaki-ng's protocol buffers.

```powershell
git submodule update --init third_party/chiaki-ng
./scripts/init-chiaki-submodules.ps1
./scripts/init-nvapi-submodule.ps1
cmake --preset release -DCMAKE_PREFIX_PATH=<Qt dir>/msvc2022_64 -DPYTHON_EXECUTABLE=<python.exe>
cmake --build --preset release
ctest --preset release
```

## Project layout

| Path                    | What it holds                                           |
| ----------------------- | ------------------------------------------------------- |
| `src/app`               | Entry point, Qt application and QML interface           |
| `src/app/assets`        | Bundled fonts, icons and images, with their licenses    |
| `src/audio`             | SDL audio output, microphone capture and haptics        |
| `src/core`              | Logging, settings and shared utilities                  |
| `src/input`             | SDL gamepads and the DualSense effect packets          |
| `src/media`             | GPU decoding (FFmpeg, D3D12VA) and the test file source |
| `src/render`            | Direct3D 12 stream window, frame presenter and shaders  |
| `src/remoteplay`        | chiaki-ng bridge: discovery, pairing, wake, saved list  |
| `tests`                 | Unit tests (Catch2)                                     |
| `third_party/chiaki-ng` | chiaki-ng, pinned as a submodule                        |
| `third_party/nvapi`     | NVIDIA's NVAPI headers, for Reflex                      |
| `cmake`, `triplets`     | CMake helpers and the vcpkg triplet                     |

## License

[GNU Affero General Public License v3.0](LICENSE), required by chiaki-ng's
library, which is AGPL-3.0 with an OpenSSL exception. NVIDIA runtimes are
downloaded at build time under NVIDIA's own licenses and are never committed;
the NVAPI headers vendored in `third_party/nvapi` and the Optical Flow headers
in `third_party/nvof` are MIT. Dear ImGui is MIT.

The interface bundles the Sora, Inter and JetBrains Mono fonts (SIL Open Font
License 1.1) and Lucide icons (ISC License); their license files sit next to
them in `src/app/assets` and ship in the package's `licenses` folder.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. NVIDIA,
RTX, DLSS and Reflex are trademarks of NVIDIA Corporation. This project is not
affiliated with or endorsed by either company.
