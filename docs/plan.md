# Lynx Play plan

A personal Windows Remote Play client for the PS5 with the best picture and the
lowest input lag: local network first, internet when needed, and NVIDIA RTX
enhancements (RTX Video Super Resolution, RTX Video HDR, DLSS Super Resolution,
DLSS Frame Generation, Reflex + Boost) on the owner's RTX 4060 Laptop GPU.

## Contents

- [Phases](#phases)
- [Decisions later phases depend on](#decisions-later-phases-depend-on)
- [Auto mode](#auto-mode)
  - [Shared by every step](#shared-by-every-step)
  - [2.3 HDR](#23-hdr)
  - [2.4 Optical flow and cadence detection](#24-optical-flow-and-cadence-detection)
  - [Decisions to review](#decisions-to-review)

## Phases

- [ ] **1. Local network streaming**: play the PS5 over the LAN with controller,
      audio and the highest bitrate the console accepts.
  - [x] **1.1 Foundation**: public repo, CI build, Qt shell, D3D12 window on the
        RTX, chiaki-lib linked.
  - [x] **1.2 Consoles** [auto]: LAN discovery, wake from rest mode, pairing
        (Account ID + PIN), saved consoles, manual IP, screens from the Drafia
        design.
  - [x] **1.3 Video** [auto]: streaming session, hardware H.265/H.264 decode
        straight into D3D12, low-latency presentation, 1080p60 at the highest
        bitrate.
  - [x] **1.4 Audio, DualSense and microphone** [auto]: low-latency audio;
        DualSense over USB and Bluetooth (haptics, adaptive triggers, touchpad,
        gyro); microphone with mute.
  - [x] **1.5 Keyboard/mouse, overlay and stats** [auto]: key mapping,
        in-stream menu, statistics and latency tuning.
- [ ] **2. RTX features**: image enhancement and lower latency on the RTX.
  - [x] **2.1 Reflex + Boost and the effects pipeline** [auto], with GPU time
        per stage.
  - [x] **2.2 RTX Video Super Resolution** [auto]: artifact cleanup at 1080p
        and upscaling to any output (1440p/4K monitors).
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
  without touching the renderer: sources hand decoded GPU frames to the
  renderer; encoded sources (the PS5, a file) go through the decoder first.
- Decoding: FFmpeg with D3D12VA (zero copy), on the renderer's device.
- Input and audio: SDL3 (vcpkg 3.4.20) for the DualSense (USB and Bluetooth),
  keyboard/mouse, audio output, haptics and microphone. Gamepads are owned by
  the application's `InputController`, not the session, because SDL pumps
  events on the main thread and the rail shows the controller outside
  sessions.
- RTX: Reflex + Boost through NVAPI — NVIDIA's `nvapi` repository pinned as a
  submodule (MIT, headers only), with `System32\nvapi64.dll` loaded at run
  time through `nvapi_QueryInterface` and no import library (2.1); RTX Video
  Super Resolution and RTX Video HDR through the driver's D3D11 video
  processor extensions on a native D3D11 device sharing textures and a fence
  with D3D12 (2.2); DLSS Super Resolution and Frame Generation through NGX,
  with runtimes downloaded in CI from NVIDIA's public releases; motion vectors
  from the driver's optical flow (NvOF).
- The in-stream overlay is Dear ImGui (vcpkg, DX12 + Win32 backends), brought
  in by 2.1 as a minimal statistics panel; 1.5 owns its final look.
- Frame Generation detects the repeated frames of 30 fps games in the 60 fps
  stream and interpolates between the distinct ones.
- Target displays: today an Acer 1080p 60 Hz monitor on the RTX (and the 240 Hz
  laptop panel on the Intel GPU); later a 1440p+ 120 Hz+ monitor. Every output
  resolution and refresh rate must work.
- Internet: PSN NAT traversal plus a manual address.
- Builds only in GitHub Actions; the app is tested by running the downloaded
  artifact on the owner's machine.

## Auto mode

Scope: up to 2.6 (extended from 2.2 at the owner's request, so DLSS
and frame generation are included; phase 3 waits). Started on 2026-11-22 while
the owner slept; nobody answers questions. Delivery: each step works on the branch
`auto/<step>` (for example `auto/1.2`), commits and pushes each delivery there
so CI builds it, and the orchestrator fast-forwards `main` to the branch once
the step is done and green.

### Shared by every step

- **Code standard:** the `craft` skill with `references/code.md`,
  `stacks/cpp.md`, `references/verify.md` and, for docs, `references/docs.md`.
  QML has no stack note: apply `code.md`'s readability rules to it (one
  property per line, small components, no logic in bindings beyond a short
  expression).
- **Read first:** `CLAUDE.md`, `README.md`, this plan, and the code of the steps
  before yours.
- **Building:** never compile on this machine. Push to `auto/<step>`; the
  `Build` workflow runs on `main` and `auto/**` branches. Follow runs with
  `gh run watch <id> --exit-status` and read failures with
  `gh run view <id> --log-failed`. A warm run takes about 10 minutes; a new
  vcpkg dependency adds its first build (FFmpeg can take 30+ minutes once).
- **Checking on this machine:** it is the target machine (RTX 4060 Laptop,
  driver 621.14, i9-14900HX, Windows 11; displays: Acer 1080p up to 75 Hz on
  the RTX, laptop panel 1920×1200 up to 240 Hz on the Intel GPU; Ethernet).
  Download the artifact into a new folder under the session scratchpad
  (`gh run download <id> --name lynx-play-windows-x64 --dir <folder>`) and run
  `lynx-play.exe`. The log is `%LOCALAPPDATA%\LynxPlay\logs\lynx-play.log`.
  Screenshots: PowerShell with `System.Drawing` (`CopyFromScreen`) into the
  scratchpad, then read the PNG. Clicking: UI Automation (Qt Quick exposes
  accessible names) or keyboard input through `user32` `SendInput` after
  `SetForegroundWindow`. Always close the app when done.
- **The owner is away:** the PS5 pairing PIN appears on the console's screen and
  the Account ID belongs to the owner, so pairing, waking and streaming from the
  real console can't be checked tonight. Those criteria are marked "owner
  checks" and are reported as not checked. Everything else is checked here: CI,
  unit tests, the app's behavior, and the video path through the developer file
  source (1.3).
- **Developer test media:** a portable FFmpeg build may be downloaded into the
  scratchpad (for example BtbN's `ffmpeg-master-latest-win64-gpl.zip`) to
  generate test streams such as
  `ffmpeg -f lavfi -i testsrc2=size=1920x1080:rate=60 -t 20 -c:v libx265 -x265-params keyint=60 test1080p60.h265`.
  Never commit media or tools.
- **Guardrails for every step:** no work beyond the step's mission; no change to
  `docs/plan.md` or `docs/issues.md` (report issues instead); never commit to
  `main`, never force-push, never rewrite pushed history; no NVIDIA or other
  third-party binaries, SDKs, fonts without a bundling license, or media in the
  repository; no PSN account IDs, console keys, PINs or tokens in the
  repository, logs or test fixtures (use obviously fake values); no capture
  card code; no `Co-Authored-By` or any AI credit in commits or files.
- **Commits:** Conventional Commits in English, one per delivery, pushed to the
  step branch right away.
- **Design:** the Drafia document `design/lynx-play.drafia` is the source of
  truth for how the app looks. Read it through the Drafia MCP tools
  (`read_skill` `guide/design-to-code.md` first). Its variables become a QML
  `Theme` singleton; its components become QML components.
- **Stuck:** after three CI attempts at the same failure with no progress, or
  when the step can't fit its run, finish the current delivery, stop and
  report what is left.


### 2.3 HDR

- **Goal:** the PS5's native HDR stream plays with correct BT.2020/PQ color on
  an HDR display and tone-maps on an SDR one, and RTX Video HDR can turn an
  SDR stream into HDR when the display and the NVIDIA App allow.
- **Guardrails:** no DLSS (2.5, 2.6); the SDR path stays exactly as it is
  when HDR is off; no capture card.
- **Success criteria:**
  1. CI green on `auto/2.3`, unit tests included.
  2. `--play-file` with an HDR10 HEVC file (P010, BT.2020/PQ) plays; the log
     shows P010, the stream's `color_trc`/`color_primaries` (and mastering
     data when present), the display's HDR state, and which presentation path
     ran; a screenshot shows a correctly tone-mapped picture on this SDR panel
     (no washed-out or crushed frame).
  3. The HDR setting (Desligado / Automático / RTX Video HDR) exists on
     Configurações › Vídeo, is saved in `settings.json`, and applies live
     where it can.
  4. With HDR on for the session, the stream request uses
     `CHIAKI_CODEC_H265_HDR` (the log shows it); P010 reaches the decoder
     (owner check for a real session).
  5. On this machine, display detection reports SDR and the app chooses the
     SDR swap chain; RTX Video HDR is disabled with an explanation. On an HDR
     display (owner check), the PQ swap chain and the TrueHDR extension run.
- **Deliveries:**
  1. HDR capture and presentation: P010 through the decoder and the shader
     (BT.2020 matrix, PQ), a 10-bit intermediate, the R10G10B10A2_UNORM swap
     chain with `DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020` on HDR displays,
     and tone mapping PQ→SDR for SDR ones; the ImGui overlay converts sRGB→PQ
     on an HDR chain.
  2. Stream profile and settings: `CHIAKI_CODEC_H265_HDR` when the setting
     asks for the stream's own HDR; `video.hdr` (off/auto/rtx) in
     `settings.json`; the Vídeo screen row.
  3. RTX Video HDR through the D3D11 extension already in place (`GUID
     {fdd62bb4-620b-4fd7-9ab3-1e59d0d544b3}`, struct `{version 4, method 3,
     enable 1}`), ordered before VSR in the same blit, with its own stage time
     in `GpuTimer` and the overlay.
  4. Display HDR detection: `DisplayConfigGetDeviceInfo` with
     `GET_ADVANCED_COLOR_INFO_2` (or matching the HMONITOR across adapters,
     since `GetContainingOutput` misses a panel on another GPU) and the
     setting's enabled/disabled explanation.
- **Settled in planning:** research in `scratchpad/research/hdr.md`: chiaki
  1.10.1 requests HDR purely through the video profile's codec
  (`CHIAKI_CODEC_H265_HDR`), with no callback flag, and FFmpeg supplies the
  color metadata; this laptop's panel is SDR and owned by the Intel GPU, so
  RTX Video HDR's picture is an owner check on an HDR display; the NVIDIA App
  needs its HDR/Video Enhancements toggle on.
- **Changes:** `src/render/` (shader, swap chain color space, HDR stage,
  overlay), `src/media/` (P010 metadata), `src/remoteplay/` (profile),
  `src/core/` (settings), `src/app/` (Vídeo screen), tests.
- **Data and interfaces:** `settings.json` gains `video.hdr`
  (`off`/`auto`/`rtx`); the decoded frame carries its color space and
  mastering data; the presentation layer reports the display's color mode.
- **Edge cases:** SDR display with HDR off and on; HDR display with Windows
  HDR off (Windows tone maps; the log says which path); P010 decode failure
  (fall back to 8-bit with a logged reason); overlay legibility on HDR;
  non-NVIDIA GPU (RTX HDR disabled).

### 2.4 Optical flow and cadence detection

- **Goal:** the renderer computes optical flow on the GPU and marks every
  source frame as distinct or repeated, estimating the stream's cadence
  (30/60 fps), shown in the overlay; the flow is exposed for DLSS in 2.5 and
  2.6.
- **Guardrails:** no DLSS or frame generation yet; with optical flow off, the
  pipeline behaves exactly as 2.2's; no capture card.
- **Success criteria:**
  1. CI green on `auto/2.4`, unit tests included (cadence state machine:
     windows, period, thresholds, resets; flow-vector scaling/densify math).
  2. `--play-file` with a 30 fps stream carried in a 60 fps container: the
     log and the overlay report cadence 30 and the repeated frames, the flow
     stage's time, and the picture is unchanged; with a true 60 fps file:
     cadence 60 and no repeated frames (screenshots of the overlay).
  3. The overlay shows the optical-flow stage's GPU time and the cadence with
     its repeated-frame count.
  4. `rtx.opticalFlow` (Ligado/Desligado, default on) on the RTX screen,
     saved and applied live.
  5. Owner check: a real 30 fps game reports cadence 30.
- **Deliveries:**
  1. NVIDIA Optical Flow: headers vendored from SDK 5.0.7 into
     `third_party/nvof` (MIT per file, with the license; the SDK zip needs a
     login, so commit the headers, never the DLL), the D3D12 session
     (`NvOFAPICreateInstanceD3D12(0x50, …)`, `nvCreateOpticalFlowD3D12`,
     `nvOFRegisterResourceD3D12`/`nvOFExecuteD3D12`, fence points), input
     BGRA8 S10.5 flow `R16G16_SINT` and cost `R8_UINT` at grid 4×4, loaded
     from `System32\nvofapi64.dll`.
  2. Flow stage in the pipeline (the presenter's intermediate becomes BGRA8),
     with its time in `GpuTimer` and the overlay.
  3. Cadence detection: a compute shader comparing the current and previous
     luma in 4×4 blocks with a group reduction and one atomic, a 16-byte
     readback fenced and read on the next frame; the state machine (a
     repeated frame when under ~1% of blocks change; a 60-flag window, at
     least 30 flags and 12 distinct, two windows agreeing within 1.6 fps;
     reset on scene cut, resize, source change and dropped frames).
  4. Setting, the RTX screen row and the overlay's cadence line.
- **Settled in planning:** research in `scratchpad/research/nvof-ngx-dlss.md`:
  the flow is only computed on `B8G8R8A8_UNORM` inputs at a 4×4 grid; the
  estimated cost at 1080p on a 4060 is ~1.0–1.3 ms (medium) and ~2.0–2.4 ms
  (slow); cadence needs a one-frame decision latency and no present stall.
- **Changes:** `src/render/` (flow, cadence shader and state), `third_party/nvof`,
  `src/core/` (settings), `src/app/` (RTX row), tests.
- **Data and interfaces:** `settings.json` gains `rtx.opticalFlow`; each
  frame carries a distinct/repeated flag and the window's cadence estimate;
  the flow texture and cost are available to the DLSS steps.
- **Edge cases:** `nvofapi64.dll` absent or too old (flow disabled with a
  logged reason; cadence falls back to the block-difference metric alone);
  30 fps source (no repeats); scene cuts; dropped frames; resize; HDR frames
  (flow on the 8-bit SDR-converted intermediate).

### Decisions to review

#### D1 – Scope of the unattended run (all)

The owner asked for everything to be done overnight without interaction, but
pairing and streaming from the real PS5 need the owner (the PIN appears on the
console's screen), and the PSN login (3.2) needs their account.

- A. Up to 1.5 (all of local streaming): stops before the RTX work.
- B. Up to 2.2 (local streaming, Reflex and VSR), every step verifiable here
  through the developer file source.
- C. The whole plan: 3.2 can't be done without the owner's PSN login, and
  2.3–2.6 depend on results not yet seen.

**Chosen: B**, because it is the furthest every step can be built and checked
on this machine. Changing it: nothing is redone; the run just stops earlier or
later.

#### D2 – Step branches instead of commits on `main` (all)

Builds only run in CI, so each step must push to compile; auto mode wants one
delivery per step on `main`.

- A. Each step on `auto/<step>`, one commit per delivery pushed there, `main`
  fast-forwarded when the step is green: `main` only gets working steps, and
  every delivery is still committed and pushed as the owner asked.
- B. Every delivery straight on `main`: `main` may hold broken builds
  overnight.

**Chosen: A**, because `main` stays green and nothing is lost. Changing it:
delete the merged `auto/*` branches; no code changes.

#### D3 – Keeping the laptop awake (all)

The power plan suspends the laptop after 15 minutes idle on AC, which would
stop the run, and the `keep-awake` skill is not installed.

- A. A background process holding `SetThreadExecutionState` for up to 12 hours,
  ended with the run: no setting changed.
- B. Change the power plan: persists after the run.
- C. Do nothing: the run stops in 15 minutes.

**Chosen: A**, because it is temporary and changes nothing. Changing it: none.

#### D4 – Dark theme only (1.2 design)

- A. Dark only: one set of screens, suits a streaming app used at night or on a
  TV.
- B. Dark and light.

**Chosen: A**. Changing it: add light values to every `Theme` color.

#### D5 – Presentation default "VSync" with a one-frame queue (1.3)

- A. VSync with at most one queued frame: no tearing, up to one refresh of
  added latency.
- B. "Menor latência": present immediately and allow tearing.

**Chosen: A** as the default, B selectable in Configurações › Vídeo, because
tearing on the current 60 Hz monitor is very visible. Changing it: the default
value in the settings store.

#### D6 – SDL3 for audio output and microphone (1.4)

- A. SDL3 for gamepad, haptics, audio output and microphone: one library,
  proven by chiaki-ng, low enough latency (WASAPI underneath).
- B. WASAPI directly for audio: a few milliseconds less, more code.

**Chosen: A**. Changing it: replace the audio output module.

#### D7 – Logo: amber eye between lynx ears (design)

- A. Eye between ears: reads at small sizes.
- B. Faceted lynx head: more detailed, muddled when small.

**Chosen: A**. It is a raster image; it needs vectorizing before the `.ico`.
Changing it: regenerate the mark and replace it in the design and the app.

#### D8 – Three font families (design)

- A. Sora for titles and the wordmark, Inter for the interface, JetBrains Mono
  for numbers (IP, PIN, overlay statistics): more personality, stable digits.
  All three are OFL and can be bundled.
- B. Inter only: simpler, smaller package.

**Chosen: A**. Changing it: swap the `font/*` variables and the bundled fonts.

#### D9 – A PS5-like console render on the cards (design)

- A. A generated render resembling a PS5 (no logos): instantly reads as "the
  console".
- B. Only the flat console glyph: neutral, no resemblance to Sony's design.

**Chosen: A**. Changing it: point the card's image at the glyph.

#### D10 – Controller focus model and overlay shortcuts (design, 1.5)

- A. The whole card takes focus; Cross runs the main action, Triangle opens
  options, Circle cancels; overlay on F1 (menu) and F2 (statistics), and
  Create + Options held on the controller.
- B. L3 + R3 for the overlay: clashes with games that use the stick clicks
  together.

**Chosen: A** (1.5's mission now uses these shortcuts). Changing it: the
bindings in 1.5.

#### D11 – Smaller design choices (design)

Rest mode badge yellow (#E9C46A) close to the amber accent, kept apart by a
moon icon and text; danger buttons soft (red text on a tinted fill); dark
toggle thumb on amber; Configurações with its own section column; immersive
connecting screen without the rail; invalid forms keep their buttons enabled
and show the error on the field.

- A. As designed.
- B. Any of them reversed (distinct rest color, solid danger, white thumb,
  sections in the main rail, rail on the connecting screen, disabled
  buttons).

**Chosen: A**. Changing it: the design file and the matching QML component.

#### D12 – A minimal Configurações page until 1.3 (1.2)

The home design has no place for 1.1's diagnostics and "Teste de vídeo", and
1.3 must keep the test working.

- A. A minimal Configurações page in the rail with the version, Remote Play
  state, GPU and the video test.
- B. A hidden keyboard shortcut: the owner wouldn't find it.
- C. Remove the test: breaks 1.3's guardrail.

**Chosen: A**. Changing it: 1.3 replaces `qml/settings/SettingsPage.qml` with
the design's Vídeo and Sobre sections and moves the test there.

#### D13 – DualSense status and controller hints hidden until 1.4 (1.2)

- A. Hide the rail's DualSense status and the controller hint bar until
  controllers are detected.
- B. Show them statically as designed: claims a connection and shortcuts that
  don't exist yet.

**Chosen: A**, because it shows nothing false. Changing it: 1.4 adds both.

#### D14 – "Jogar" before streaming exists (1.2)

- A. Keep the buttons as designed and show a toast that streaming is not there
  yet.
- B. Disable or hide them, departing from the design.

**Chosen: A**. Changing it: `ConsolesController::play` in 1.3.

#### D15 – nlohmann-json for the console store (1.2)

- A. nlohmann-json (vcpkg, header-only): `remoteplay` and its tests stay free
  of Qt.
- B. `QJsonDocument`: pulls Qt Core into `remoteplay` and the tests.

**Chosen: A**. Changing it: rewrite `ConsoleStore.cpp`.

#### D16 – "Parear" in the card's menu (1.2)

A console added by IP that doesn't answer discovery would never show the
"Parear" button, yet pairing by IP is a real use case (chiaki-ng allows it).

- A. A "Parear" item in the "…" menu of every unpaired console.
- B. Pair only consoles that answer discovery.

**Chosen: A**. Changing it: remove that menu row in `ConsolesPage.qml`.

#### D17 – Report a session's end from the interface, not the video thread (1.3)

- A. Call `chiaki_session_stop` from the video callback: risks a deadlock.
- B. Report the end to the interface and stop the session from the UI thread.

**Chosen: B**. Changing it: call stop in the callback and drop the `endReported`
guard.

#### D18 – Cancel only while connecting (1.3)

- A. Cancel at any session phase.
- B. Cancel and Esc act only in Connecting, StartingVideo and Failed, because
  Esc belongs to the stream window during playback.

**Chosen: B**. Changing it: remove the three-line phase guard.

#### D19 – One gamepad manager in the application, not in the session (1.4)

`SDL_PumpEvents` must run on the main thread and 1.2's rail needs controller
status outside sessions.

- A. A `GamepadManager` in the application's `InputController`, with the
  session reading snapshots.
- B. The session owns SDL: opens duplicate handles and needs another status
  path for the UI.

**Chosen: A**. Changing it: move the manager into `StreamSession` and add a
status source for the interface.

#### D20 – Haptics over the USB audio endpoint, rumble as fallback (1.4)

- A. Four-channel USB audio for fine haptics; classic rumble over Bluetooth
  or when the endpoint isn't 4ch, and rumble events ignored while the audio
  haptics are active.
- B. Always rumble: loses the fine haptics.
- C. Always audio haptics: silence or the wrong path over Bluetooth.

**Chosen: A** (the research confirmed `SDL_RumbleGamepad` disables audio
haptics). Changing it: `Haptics` and `StreamSession`'s rumble branch.

#### D21 – Game volume in `settings.json` (1.4)

The design sketch has an 85% volume the data model didn't mention.

- A. `audio.volumePercent`, applied as gain on the SDL stream.
- B. Drop the design's line.

**Chosen: A**, the design is the source of the screen. Changing it: remove the
field and the gain.

#### D22 – Device changes apply to the next session; mute and volume live (1.4)

No Apply button, and reopening the audio device mid-game is risky.

- A. Save and open on the next stream; mute and volume apply immediately.
- B. Reopen output/microphone right away.

**Chosen: A**. Changing it: reopen in `StreamController`.

#### D23 – Preferred controller by SDL name (1.4)

- A. Store the gamepad's name; the first match wins.
- B. Instance id/index: unstable across runs.

**Chosen: A**. Changing it: `ControllerSettings` and the manager's selection.

#### D24 – The overlay arrives early, with 2.1 (2.1)

2.1's criteria need per-stage GPU time and the Reflex latency report on
screen, while the full overlay is 1.5's.

- A. A minimal Dear ImGui panel now, always visible; 1.5 styles it, adds the
  F1/F2 toggles, the full statistics and the quick actions.
- B. Temporary text overlay: redone in 1.5.
- C. Log only: fails 2.1's criteria.

**Chosen: A**. Changing it: `Overlay.cpp` in 1.5.

#### D25 – Reflex on by default, Boost opt-in (2.1)

- A. On: low latency with no extra power draw; Boost stays an explicit choice.
- B. Off by default.
- C. Boost by default, as the sketch shows: extra laptop power draw on every
  install.

**Chosen: A**. Changing it: the default in `core::RtxSettings`.

#### D26 – The stall detector's threshold (2.1)

- A. Sleep longer than max(GPU work of the frame, 2 ms) in 10 of 16 frames:
  catches the measured 26 ms sleeps without flagging sub-millisecond work.
- B. Compare against the time already blocked in the frame slot: hides a 4 ms
  sleep inside a 16 ms VSync.
- C. No detector: risks the measured 31 fps case.

**Chosen: A**. Changing it: `SleepStallDetector` and `lastGpuWorkUs`.

#### D27 – The RTX screen ships only with the Reflex row (2.1)

- A. Only Reflex, the feature that exists; VSR, HDR and DLSS rows arrive
  with their steps.
- B. The sketch's rows, disabled.
- C. All rows functional now (beyond the mission).

**Chosen: A**. Changing it: add rows in each step.

#### D28 – `settings.json` stays version 1 with additive groups (2.1, kept by the merge)

- A. Version 1 with additive fields, each defaulting when absent.
- B. Version 2 with a migration, which would reject existing files and reset
  the owner's video settings on an older build.

**Chosen: A**. Changing it: bump `kSettingsVersion` and add a migration.


#### D29 – Received, decoded and presented fps on three lines (1.5)

- A. One combined line as the design shows: doesn't fit in 296 px with three
  values.
- B. Three lines: shows the criterion's three values uncut.

**Chosen: B**. Changing it: back to a single line.

#### D30 – Keep the Reflex section in the statistics panel (1.5)

- A. Remove it, losing 2.1's latency report.
- B. Keep it compact.

**Chosen: B**, so 2.1 doesn't regress. Changing it: delete the section.

#### D31 – Quick menu actions now, VSR later (1.5)

- A. Reflex, statistics, microphone and disconnect now; VSR arrives with 2.2.
- B. Add VSR/DLSS/Configurações now (VSR doesn't exist; settings would end the
  stream).

**Chosen: A**. Changing it: insert the VSR row before the spacer.

#### D32 – A key conflict swaps the two bindings (1.5)

- A. Unbind the other control.
- B. Swap the keys between the two controls, so none ends without one.

**Chosen: B**. Changing it: clear the other binding.

#### D33 – Presented fps counts only new decoded frames (1.5)

- A. Count every present, including overlay redraws (~240 fps).
- B. Count presented new decoded frames: reflects the stream.

**Chosen: B**. Changing it: count every present.

#### D34 – GPU thread priority through a minimal D3D11 device (1.5)

D3D12 exposes no `SetGPUThreadPriority`.

- A. Log the failure only.
- B. Create a minimal D3D11 device on the same adapter just for
  `IDXGIDevice`, which 2.2's interop will later share.

**Chosen: B**, the API's only path. Changing it: reuse 2.2's interop device.

#### D35 – Packet loss from cumulative deltas (1.5)

- A. `chiaki_packet_stats_get(reset = true)`: steals the congestion
  controller's samples.
- B. Read cumulative counters and compute deltas.

**Chosen: B**, so the bitrate controller isn't disturbed. Changing it: consume
with reset.

#### D36 – VSR off by default (2.2)

- A. Off by default: the stage costs ~5–7 ms a frame and the picture is a
  taste; opt-in.
- B. On by default, like Reflex: the GPU pays even with the NVIDIA App's
  global toggle off.

**Chosen: A**. Changing it: the default in `core/Settings.h` and the screen.

#### D37 – The owner's NVIDIA App was set to enable Super Resolution (2.2)

The RTX Video Enhancement global toggle was off, so VSR would return `S_OK`
and change nothing.

- A. Leave it off: the criterion's visual proof missing.
- B. Turn it on (Sistema › Vídeo › RTX Video Enhancements › Super Resolution,
  Automático): VSR works, for this and other video apps.

**Chosen: B**; this changes a machine setting the owner should know about.
Changing it: turn the toggle back off in the NVIDIA App.

#### D38 – Two-state VSR setting, no quality control (2.2)

The driver extension has no quality field; quality 1–4 lives in the NVIDIA
App.

- A. Off/On only, with a note about the NVIDIA App.
- B. The RTX Video SDK path (proprietary DLLs, NVIDIA login in CI, an RGB
  conversion pass) for quality levels.

**Chosen: A**. Changing it: implement the SDK path.

#### D39 – The VSR stage time includes the copy and the handoff (2.2)

D3D12 queries don't time the D3D11 video processor.

- A. Measure from the input copy to the output wait, labelled "VSR": the
  stage's real cost to the frame.
- B. Delta of frame-to-present with VSR on and off.

**Chosen: A**. Changing it: subtract the copy or add a second measurement.

#### D40 – Shared VSR textures are created in D3D11 first (2.2)

The D3D11 runtime refuses NV12 shared with `SHARED_NTHANDLE` only, and a
D3D12-created texture also failed.

- A. Create in D3D11 with `SHARED | SHARED_NTHANDLE`, open the NT handle in
  D3D12, with a D3D12-first fallback and a local copy when the input view
  rejects the texture.
- B. D3D12-created only: fails for NV12.

**Chosen: A**. Changing it: remove the fallback.

#### D41 – One native D3D11 device for GPU priority and VSR (merge 1.5 + 2.2)

- A. `GpuDevice` creates one native D3D11 device with `BGRA_SUPPORT |
  VIDEO_SUPPORT` on the stream's adapter; 1.5's GPU thread priority and
  2.2's video processor use it; VSR's probe keeps its temporary device.
- B. Two devices: more memory and two lifecycles.

**Chosen: A**. Changing it: split the device back out of `GpuDevice`.

#### D42 – Scope extended through 2.6 (all)

The owner asked about DLSS and frame generation, so the run no longer stops at
2.2.

- A. Build 2.3 (HDR), 2.4 (optical flow and cadence), 2.5 (DLSS Frame
  Generation 2x) and 2.6 (DLSS Super Resolution), two at a time; phase 3
  waits.
- B. Stop at 2.2 and hand the rest over.

**Chosen: A**. Changing it: nothing is redone; the run just stops earlier.
