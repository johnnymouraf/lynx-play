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
  - [1.5 Keyboard/mouse, overlay and stats](#15-keyboardmouse-overlay-and-stats)
  - [2.2 RTX Video Super Resolution](#22-rtx-video-super-resolution)
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
  - [ ] **1.5 Keyboard/mouse, overlay and stats** [auto]: key mapping,
        in-stream menu, statistics and latency tuning.
- [ ] **2. RTX features**: image enhancement and lower latency on the RTX.
  - [x] **2.1 Reflex + Boost and the effects pipeline** [auto], with GPU time
        per stage.
  - [ ] **2.2 RTX Video Super Resolution** [auto]: artifact cleanup at 1080p
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

Scope: up to 2.2. Started on 2026-11-22 while the owner sleeps; nobody answers
questions until morning. Delivery: each step works on the branch
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

### 1.5 Keyboard/mouse, overlay and stats

- **Goal:** keyboard and mouse can play with a configurable mapping, an
  in-stream overlay shows live statistics and quick actions, and the pipeline
  is tuned for latency.
- **Guardrails:** no RTX features (phase 2); the overlay is Dear ImGui drawn in
  the stream's swap chain, never a Qt window over it.
- **Success criteria:**
  1. CI green on `auto/1.5`, unit tests included.
  2. Unit tests cover the key mapping (defaults, remapping, conflicts) and the
     statistics math (rates and averages over a sliding window).
  3. In `--play-file` playback the overlay opens and closes with the design's
     shortcuts (F1 menu, F2 statistics; Create + Options held on the
     controller) and shows resolution, codec, received/decoded/presented fps,
     bitrate, decode time and present-to-present time, updating live
     (screenshots).
  4. The Controles screen edits and saves the keyboard/mouse mapping.
  5. The log confirms the latency measures in effect: MMCSS for the decode
     and render threads, power throttling disabled for the process, the GPU
     thread priority, and the frame queue depth of 1.
  6. Owner checks: playing with keyboard and mouse; packet loss and bitrate
     readings during a real session.
- **Deliveries:**
  1. Finish the overlay 2.1 brought: the design's styling (Inter labels,
     mono values, 320 wide), the F1 menu / F2 statistics toggles and a hidden
     by default (2.1 leaves a minimal panel always visible), and the quick
     actions (Reflex, VSR, disconnect).
  2. Statistics: counters from the session, decoder and renderer, shown in
     the overlay.
  3. Keyboard/mouse: raw input in the stream window mapped to controller
     state (mouse to right stick with sensitivity), mapping saved in
     `settings.json`, edited on the Controles screen.
  4. Latency tuning: MMCSS, process power throttling off, GPU priority,
     decoder flags review, input polled right before sending.
- **Settled in planning:** 2.1 already brings Dear ImGui (DX12 + Win32
  backends), a minimal always-visible statistics panel (stage GPU times,
  Reflex mode and latency report, JetBrains Mono 15 px, 360 wide) and
  `settings.json`'s `rtx.reflex`; this step styles and completes the overlay
  and adds the toggles it lacks (decision D24). The Controles screen already
  exists from 1.4 and gains the keyboard/mouse mapping.
- **Changes:** `src/render/` (overlay), `src/input/` (keyboard/mouse),
  `src/core/` (statistics), `src/app/` (Controles screen), `vcpkg.json`
  (imgui), tests.
- **Data and interfaces:** `settings.json` gains `keyboardMouse` (bindings,
  mouse sensitivity, invert Y) and `overlay` (stats visible).
- **Edge cases:** the overlay open while the controller is used (input goes to
  the overlay, not the game); window focus lost (release every key);
  very high mouse polling rates.

### 2.2 RTX Video Super Resolution

- **Goal:** RTX Video Super Resolution upscales and cleans the stream on RTX
  GPUs, for the current 1080p monitor (artifact cleanup) and future 1440p/4K
  monitors (upscaling).
- **Guardrails:** no HDR (2.3); no DLSS (2.5, 2.6).
- **Success criteria:**
  1. CI green on `auto/2.2`, unit tests included.
  2. In `--play-file` playback of a 720p stream in a 1080p window with VSR on,
     the log shows the NVIDIA VSR extension accepted on the video processor,
     the overlay shows the VSR stage's GPU time, and a screenshot comparison
     with VSR off shows a visible difference.
  3. With VSR off, or on a non-NVIDIA GPU, the pipeline scales with the
     shader path and the setting is disabled with an explanation.
  4. The RTX screen has the VSR setting (Desligado / Ligado, quality if the
     API allows), saved and applied live.
  5. Owner checks: picture quality in real games at 1080p.
- **Deliveries:**
  1. D3D11 interop: a D3D11 device on the same adapter, shared textures (NT
     handles) and a shared fence with D3D12.
  2. VSR stage: `ID3D11VideoProcessor` with NVIDIA's stream extension
     (`VideoProcessorSetStreamExtension`), scaling to the output size.
  3. Setting, detection (NVIDIA GPU, driver support) and the RTX screen
     part.
- **Settled in planning:** VSR is implemented through the driver's D3D11 video
  processor extension, so no SDK is needed; the RTX Video SDK path is not
  used. Confirmed by research (`scratchpad/research/rtx-vsr.md`):
  `ID3D11VideoContext::VideoProcessorSetStreamExtension` on a **native**
  D3D11 device on the stream's adapter (D3D11On12 returns `E_NOTIMPL`), GUID
  `{d43ce1b3-1f4b-48ac-baee-c3c25375e6f7}`, struct
  `{version = 1; method = 2; enable = 1}` (12 bytes, no quality field — the
  quality and the on/off global toggle live in the NVIDIA App), NV12/P010 in
  and R8G8B8A8 out, 1:1 clean-up supported, downscale not; textures shared
  with NT handles plus one shared fence (D3D12 signal → D3D11 wait → blit →
  D3D11 signal/flush → D3D12 wait), the input view must bind render target and
  shader resource; expect ~5–8 ms per 1080p→1440p/4K frame on the 4060 and
  measure. The extension returns `S_OK` even with the app's global toggle off,
  so the picture is the only proof.
- **Changes:** `src/render/` (D3D11 interop, VSR stage), `src/app/` (RTX
  screen), tests for the pure parts (size and rectangle math).
- **Data and interfaces:** `settings.json` gains `rtx.videoSuperResolution`.
- **Edge cases:** the global "RTX Video Enhancement" switch in the NVIDIA App
  off (detect if possible, explain in the setting); 1:1 scale (artifact
  cleanup only); output smaller than input; device lost.

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

