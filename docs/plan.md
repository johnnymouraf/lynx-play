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
  - [1.4 Audio, DualSense and microphone](#14-audio-dualsense-and-microphone)
  - [1.5 Keyboard/mouse, overlay and stats](#15-keyboardmouse-overlay-and-stats)
  - [2.1 Reflex + Boost and the effects pipeline](#21-reflex--boost-and-the-effects-pipeline)
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
  - [ ] **1.4 Audio, DualSense and microphone** [auto]: low-latency audio;
        DualSense over USB and Bluetooth (haptics, adaptive triggers, touchpad,
        gyro); microphone with mute.
  - [ ] **1.5 Keyboard/mouse, overlay and stats** [auto]: key mapping,
        in-stream menu, statistics and latency tuning.
- [ ] **2. RTX features**: image enhancement and lower latency on the RTX.
  - [ ] **2.1 Reflex + Boost and the effects pipeline** [auto], with GPU time
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
- Input and audio: SDL3 for the DualSense (USB and Bluetooth), keyboard/mouse,
  audio output, haptics and microphone.
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

### 1.4 Audio, DualSense and microphone

- **Goal:** the stream plays the console's audio with low latency, the DualSense
  controls the PS5 over USB and Bluetooth with haptics, adaptive triggers,
  touchpad and gyro, and the microphone reaches the console with a mute
  toggle.
- **Guardrails:** no keyboard/mouse mapping yet (1.5); no overlay (1.5).
- **Success criteria:**
  1. CI green on `auto/1.4`, unit tests included.
  2. Unit tests cover the pure mappings: SDL gamepad state (buttons, sticks,
     triggers, touchpad points, gyro/accel) to `ChiakiControllerState`;
     chiaki feedback events (rumble, trigger effects) to the DualSense output
     commands; haptic audio routing (USB) and its rumble fallback
     (Bluetooth).
  3. With a DualSense connected to this PC (if one is connected tonight), the
     log shows it detected with its connection type; otherwise record that it
     could not be checked.
  4. The Áudio e microfone and Controles screens from the design list the
     output and microphone devices and the DualSense status, and save the
     choices in `settings.json`.
  5. Owner checks: audio in sync and without crackling during a real session;
     controller input, haptics, adaptive triggers, touchpad and gyro in game;
     voice reaching the console; mute working.
- **Deliveries:**
  1. SDL3 from vcpkg (static), initialized for gamepad, audio and sensors;
     device listing for the settings screens.
  2. Audio output: chiaki's decoded PCM (48 kHz) through an SDL audio stream
     with a small, bounded queue (drop old audio rather than grow latency).
  3. DualSense input: SDL gamepad events and sensors to
     `chiaki_session_set_controller_state`, touchpad included.
  4. Feedback: rumble, adaptive triggers (DualSense effect packets through
     `SDL_SendGamepadEffect`), lightbar, haptics over USB (the controller's
     audio channels) with rumble fallback over Bluetooth.
  5. Microphone: SDL recording device → `chiaki_session_connect_microphone`
     and the Opus sender; mute toggle bound to the DualSense mic button and
     the interface.
- **Settled in planning:** DualSense over USB and Bluetooth; over Bluetooth
  fine haptics may fall back to plain rumble; microphone from the DualSense or
  a PC headset. From 1.2 (D13): add the rail's DualSense status and the
  controller hint bar from the design, shown only while a controller is
  detected.
- **Changes:** a new `src/input/` (gamepad, mappings), `src/audio/` (output,
  microphone), `src/remoteplay/` (session hooks), `src/app/` (settings
  screens), `vcpkg.json` (sdl3), `tests/input/`, `tests/audio/`. Reference:
  `third_party/chiaki-ng/gui/src/controllermanager.cpp` and
  `streamsession.cpp` (haptics, triggers, microphone).
- **Data and interfaces:** `settings.json` gains `audio` (output device,
  microphone device, microphone muted) and `controller` (preferred
  controller) objects.
- **Edge cases:** controller connected or disconnected mid-session; two
  controllers (first one wins, show it); no audio device or the device
  removed; microphone permission denied by Windows privacy settings (clear
  message); Bluetooth latency.

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
  1. Dear ImGui (vcpkg, DX12 + Win32 backends) in the stream window, styled
     with the design's tokens; overlay toggle.
  2. Statistics: counters from the session, decoder and renderer, shown in
     the overlay.
  3. Keyboard/mouse: raw input in the stream window mapped to controller
     state (mouse to right stick with sensitivity), mapping saved in
     `settings.json`, edited on the Controles screen.
  4. Latency tuning: MMCSS, process power throttling off, GPU priority,
     decoder flags review, input polled right before sending.
- **Settled in planning:** none beyond the shared decisions.
- **Changes:** `src/render/` (overlay), `src/input/` (keyboard/mouse),
  `src/core/` (statistics), `src/app/` (Controles screen), `vcpkg.json`
  (imgui), tests.
- **Data and interfaces:** `settings.json` gains `keyboardMouse` (bindings,
  mouse sensitivity, invert Y) and `overlay` (stats visible).
- **Edge cases:** the overlay open while the controller is used (input goes to
  the overlay, not the game); window focus lost (release every key);
  very high mouse polling rates.

### 2.1 Reflex + Boost and the effects pipeline

- **Goal:** the renderer runs its work as a chain of GPU stages with a timing
  for each, and NVIDIA Reflex (Off / On / On + Boost) trims the render queue
  latency on RTX GPUs.
- **Guardrails:** no image enhancement yet (2.2+); Reflex is skipped cleanly on
  non-NVIDIA GPUs.
- **Success criteria:**
  1. CI green on `auto/2.1`, unit tests included.
  2. The overlay shows the GPU time of each stage (color conversion, scaling,
     overlay) from timestamp queries, in `--play-file` playback.
  3. With Reflex On + Boost, the log shows `NvAPI_D3D_SetSleepMode` succeeding
     with boost, sleep and latency markers per source frame, and the overlay
     shows Reflex's latency report (`NvAPI_D3D_GetLatency`).
  4. The RTX screen from the design has the Reflex setting, saved in
     `settings.json`, applied without restarting the stream.
  5. Owner checks: input feels at least as responsive with Reflex on.
- **Deliveries:**
  1. NVAPI: NVIDIA's `nvapi` repository as a pinned submodule (MIT), linked
     only when building for x64.
  2. Effects pipeline: ordered GPU stages between the decoded frame and the
     back buffer, each with timestamp queries.
  3. Reflex: sleep mode, one `NvAPI_D3D_Sleep` per source frame, latency
     markers, latency report in the overlay.
  4. The RTX settings screen (Reflex part).
- **Settled in planning:** Reflex + Boost is wanted; Boost raises power use on
  a laptop, so it is a separate choice.
- **Changes:** `src/render/` (pipeline, Reflex session), `src/app/` (RTX
  screen), `.gitmodules`, `cmake/`.
- **Data and interfaces:** `settings.json` gains `rtx.reflex`
  (`off`/`on`/`boost`).
- **Edge cases:** Intel GPU or missing NVAPI (setting disabled, explained);
  driver rejecting sleep mode; frame pacing when the source stalls (no
  sleep-driven stalls: one sleep per source frame only).

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
  used.
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

