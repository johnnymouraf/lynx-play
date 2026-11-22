# Lynx Play plan

A personal Windows Remote Play client for the PS5 with the best picture and the
lowest input lag: local network first, internet when needed, and NVIDIA RTX
enhancements (RTX Video Super Resolution, RTX Video HDR, DLSS Super Resolution,
DLSS Frame Generation, Reflex + Boost) on the owner's RTX 4060 Laptop GPU.

## Contents

- [Phases](#phases)
- [Decisions later phases depend on](#decisions-later-phases-depend-on)
- [Current: 1.1 Foundation](#current-11-foundation)

## Phases

- [ ] **1. Local network streaming**: play the PS5 over the LAN with controller,
      audio and the highest bitrate the console accepts.
  - [ ] **1.1 Foundation**: public repo, CI build, Qt shell, D3D12 window on the
        RTX, chiaki-lib linked.
  - [ ] **1.2 Consoles**: Drafia design of every screen; LAN discovery, wake
        from rest mode, registration (Account ID + PIN), saved consoles, manual
        IP.
    - Account ID is typed in (with a link explaining how to find it); the PSN
      login in 3.2 fills it automatically later.
    - Starts with the design: sketch of the whole product, production fidelity
      for this milestone's screens.
  - [ ] **1.3 Video**: streaming session, hardware H.265/H.264 decode straight
        into D3D12, low-latency presentation, 1080p60 at the highest bitrate.
    - Defaults: 1080p, 60 fps, H.265, the highest bitrate the PS5 accepts on
      the LAN (found empirically).
  - [ ] **1.4 Audio, DualSense and microphone**: low-latency audio; DualSense
        over USB and Bluetooth (haptics, adaptive triggers, touchpad, gyro);
        microphone with mute.
    - Over Bluetooth, fine haptics may fall back to plain rumble.
    - Microphone from the DualSense or a PC headset.
  - [ ] **1.5 Keyboard/mouse, overlay and stats**: key mapping, in-stream menu,
        statistics and latency tuning.
- [ ] **2. RTX features**: image enhancement and lower latency on the RTX.
  - [ ] **2.1 Reflex + Boost and the effects pipeline**, with GPU time per
        stage.
  - [ ] **2.2 RTX Video Super Resolution**: artifact cleanup at 1080p and
        upscaling to any output (1440p/4K monitors).
  - [ ] **2.3 HDR**: the PS5's native HDR stream and RTX Video HDR.
  - [ ] **2.4 Optical flow and cadence detection**: find the repeated frames of
        30 fps games inside the 60 fps stream.
  - [ ] **2.5 DLSS Frame Generation 2x**: 30→60 and 60→120 fps, with Reflex.
    - Off by default: it adds about one source frame of latency.
  - [ ] **2.6 DLSS Super Resolution (experimental)**, fed by optical-flow
        motion vectors and compared with VSR.
- [ ] **3. Internet**: play away from home.
  - [ ] **3.1 Manual address and network profiles**: VPN (Tailscale/ZeroTier) or
        port forwarding; separate LAN and internet bitrates.
  - [ ] **3.2 PSN remote connection**: PSN login, NAT traversal without router
        setup, automatic LAN → PSN fallback.

Out of scope for now: capture card input (planned for the future).

## Decisions later phases depend on

- Stack: C++23, CMake, `clang-cl`, vcpkg manifest; Qt 6 QML for the interface;
  chiaki-ng's `chiaki-lib` pinned as a submodule (AGPL-3.0, so the project is
  AGPL-3.0).
- The stream renders in its own top-level Win32 window with a Direct3D 12 flip
  model swap chain (borderless fullscreen, tearing allowed). The in-stream
  overlay is drawn by Direct3D 12 (Dear ImGui), never by Qt, so presentation
  keeps independent flip.
- Video goes through a source abstraction, so a capture card can be added later
  without touching the renderer.
- Decoding: FFmpeg with D3D12VA (zero copy).
- Input: SDL3 for the DualSense (USB and Bluetooth) and keyboard/mouse.
- RTX: Reflex + Boost through NVAPI (open headers); RTX Video Super Resolution
  and RTX Video HDR through the driver's D3D11 video processor extensions,
  shared with D3D12; DLSS Super Resolution and Frame Generation through NGX,
  with runtimes downloaded in CI from NVIDIA's public releases; motion vectors
  from the driver's optical flow (NvOF).
- Frame Generation detects the repeated frames of 30 fps games in the 60 fps
  stream and interpolates between the distinct ones.
- Target displays: today an Acer 1080p 60 Hz monitor on the RTX (and the 240 Hz
  laptop panel on the Intel GPU); later a 1440p+ 120 Hz+ monitor. Every output
  resolution and refresh rate must work.
- Internet: PSN NAT traversal plus a manual address.
- Builds only in GitHub Actions; the app is tested by running the downloaded
  artifact on the owner's machine.

## Current: 1.1 Foundation

**Goal:** the public repository `lynx-play`, which GitHub Actions builds into a
portable zip that opens the Qt interface in Brazilian Portuguese and a Direct3D
12 video window on the RTX 4060, with chiaki-lib (pinned chiaki-ng) built and
linked.

**Guardrails:**

- Nothing is compiled on this machine; running the downloaded artifact is
  allowed.
- Each delivery is committed and pushed to `main`; no force-push.
- No NVIDIA or third-party binaries or SDKs in the repository.
- No personal data (Account ID, console keys) in the repository or logs.
- No capture card code.

**Success criteria:**

1. `gh repo view johnnymouraf/lynx-play` shows a public repository with the
   AGPL-3.0 license, `README.md`, `CLAUDE.md` and `docs/plan.md`.
2. The workflow passes on the Windows runner: it builds chiaki-lib and the app,
   runs the unit tests with CTest and uploads `lynx-play-windows-x64`.
3. With the dependency cache warm, a new build finishes in under 15 minutes.
4. The downloaded zip runs here without installing anything: the Qt window opens
   in Brazilian Portuguese, and the "Teste de vídeo" button opens the Direct3D
   12 window, animated, on the RTX 4060 (the log names the adapter); F11 toggles
   borderless fullscreen and Esc closes it.
5. The log shows chiaki-lib initialized successfully.

**Deliveries:**

1. Repository and docs.
2. Build and CI: CMake, vcpkg, chiaki-ng submodule, cached workflow, tests.
3. Qt QML shell in Brazilian Portuguese, file logging, zip packaging.
4. Direct3D 12 window: RTX selection, low-latency flip model swap chain,
   borderless fullscreen, animated pattern.
5. Verification on this machine against every criterion.
