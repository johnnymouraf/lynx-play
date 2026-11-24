# MFPlay plan

A personal Windows Remote Play client for the PS5 with the best picture and the
lowest input lag: local network first, internet when needed, and NVIDIA RTX
enhancements (RTX Video Super Resolution, RTX Video HDR, DLSS Super Resolution,
DLSS Frame Generation, Reflex + Boost) on the owner's RTX 4060 Laptop GPU.

## Contents

- [Phases](#phases)
- [Decisions later phases depend on](#decisions-later-phases-depend-on)
- [Current: 3.1 Rename to MFPlay](#current-31-rename-to-mfplay)
- [Auto mode](#auto-mode)
  - [Shared by every step](#shared-by-every-step)
  - [Shared by the redesign steps (3.2–3.5)](#shared-by-the-redesign-steps-3235)
  - [3.2 Theme and shared components](#32-theme-and-shared-components)
  - [3.3 Consoles and pairing](#33-consoles-and-pairing)
  - [3.4 Settings, Sobre and connecting](#34-settings-sobre-and-connecting)
  - [3.5 Stream overlay](#35-stream-overlay)
  - [Decisions to review](#decisions-to-review)

## Phases

- [ ] **1. Local network streaming**: play the PS5 over the LAN with controller,
      audio and the highest bitrate the console accepts.
  - [x] **1.1 Foundation**: public repo, CI build, Qt shell, D3D12 window on the
        RTX, chiaki-lib linked.
  - [x] **1.2 Consoles** [auto]: LAN discovery, wake from rest mode, pairing
        (Account ID + PIN), saved consoles, manual IP, screens from the
        design.
  - [x] **1.3 Video** [auto]: streaming session, hardware H.265/H.264 decode
        straight into D3D12, low-latency presentation, 1080p60 at the highest
        bitrate.
  - [x] **1.4 Audio, DualSense and microphone** [auto]: low-latency audio;
        DualSense over USB and Bluetooth (haptics, adaptive triggers, touchpad,
        gyro); microphone with mute.
  - [x] **1.5 Keyboard/mouse, overlay and stats** [auto]: key mapping,
        in-stream menu, statistics and latency tuning.
  - [x] **1.6 PSN login for the Account ID**: the pairing dialog logs in
        through the browser (OAuth redirect) and fills the Account ID;
        manual entry stays.
  - [x] **1.7 Minimizing the stream must not crash**: the overlay re-entered
        its own mutex on a click and Frame Generation kept presenting into an
        iconic window; both fixed, with the pipeline idle while hidden.
  - [x] **1.8 Closing the stream must not kill the app**: releasing the
        pictures the optical flow read faulted inside the NVIDIA driver; the
        release now happens on the render thread under a guard.
  - [x] **1.9 Controller parity with chiaki-ng**: the audit against chiaki-ng
        fixed the trigger scale, the touchpad, the sensors, the merge of
        keyboard and gamepad, and the rumble/trigger packets.
  - [x] **1.10 Stream window mode and size**: fullscreen or a chosen size
        (presets or a drag), remembered, with a usable cursor.
  - [x] **1.11 The stats panel tells the game, generated and presented rates**:
        source 60 with a 30 fps game reads game 30, generated 30, displayed
        60, with no repeated frame presented as real.
- [x] **2. RTX features**: image enhancement and lower latency on the RTX.
  - [x] **2.1 Reflex + Boost and the effects pipeline** [auto], with GPU time
        per stage.
  - [x] **2.2 RTX Video Super Resolution** [auto]: artifact cleanup at 1080p
        and upscaling to any output (1440p/4K monitors).
  - [x] **2.3 HDR**: the PS5's native HDR stream and RTX Video HDR.
  - [x] **2.4 Optical flow and cadence detection**: find the repeated frames of
        30 fps games inside the 60 fps stream.
  - [x] **2.5 DLSS Frame Generation 2x**: 30→60 and 60→120 fps, with Reflex.
    - Off by default: it adds about one source frame of latency.
  - [x] **2.6 DLSS Super Resolution (experimental)**, fed by optical-flow
        motion vectors and compared with VSR.
- [ ] **3. MFPlay: name and redesign**: the app becomes MFPlay and every
      screen follows the Drafia redesign (`design file`).
  - [ ] **3.1 Rename to MFPlay**: name everywhere (code, namespace, CMake,
        QML module, exe, workflow, docs), the vector icon and logo from the
        design, data moved from `MFPlay` to `MFPlay`, the repository renamed
        to `johnnymouraf/mfplay`.
  - [ ] **3.2 Theme and shared components** [auto]: the new `Theme` tokens (blue
        accent, radii, rail), buttons in the menu-button pattern, badges,
        fields, toggles, segmented controls, slider, dialogs, the rail with
        Sobre, the controller hint bar and the DualSense card.
  - [ ] **3.3 Consoles and pairing** [auto]: cards with the new states, status
        counters, grid/list toggle, empty state, Adicionar pelo IP,
        rename/forget, pairing with the PSN login in the new style.
  - [ ] **3.4 Settings, Sobre and connecting** [auto]: Vídeo, Áudio, Controles (with
        the DualSense battery), RTX, Rede (its internet rows disabled until
        phase 4), Sobre in the rail, Conectando and its error.
  - [ ] **3.5 Stream overlay** [auto]: the design's statistics panel and Menu rápido
        in Dear ImGui, keeping every row the current overlay has.
  - Settled for the whole phase: where the design covers something, its layout
    and copy replace the code's; what exists only in the code stays, styled
    like the design; where the design simplifies a behavior (VSR and DLSS SR
    as separate toggles, though exclusive), the real behavior stays with the
    design's look. The Rede page's internet bitrate and remote address are
    shown disabled, saying they arrive with internet play; its PSN account
    login saves the Account ID that pairing then fills in.
- [ ] **4. Internet**: play away from home.
  - [ ] **4.1 Manual address and network profiles**: VPN (Tailscale/ZeroTier) or
        port forwarding; separate LAN and internet bitrates.
  - [ ] **4.2 PSN remote connection**: PSN login, NAT traversal without router
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
- NGX (DLSS Super Resolution and Frame Generation) runs through one
  `NgxHost` created on first need, with `nvngx_dlss.dll` and `nvngx_dlssg.dll`
  310.9.16 fetched in CI from NVIDIA's public release and shipped beside the
  executable; Frame Generation requires Ada and HAGS, forces Reflex on, and
  presents generated frames through the out-of-band queue. The upscaler is one
  setting (shader / VSR / DLSS).
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

## Current: 3.1 Rename to MFPlay

- **Goal:** the project, the app and the repository are called MFPlay;
  "Lynx" remains only in the migration of the old data folders and in git
  history.
- **Guardrails:** no local build; nothing in `third_party/chiaki-ng`; no
  force-push; commit and push to `main` after each delivery; screen layouts
  don't change yet (only the logo and the icon); the repository is renamed
  only once the renamed code is green.
- **Success criteria:**
  1. `git grep -i lynx` finds only the migration of the old folders and the
     DPAPI entropy, which must keep its value for saved secrets to decrypt.
  2. CI is green; the artifact `mfplay-windows-x64` holds `mfplay.exe` with the
     MF icon.
  3. With a `MFPlay` folder in `%APPDATA%` or `%LOCALAPPDATA%`, the first run
     copies it to `MFPlay` and shows the paired consoles; the old folder stays
     untouched (checked by running the app on this machine, which has real
     `LynxPlay` data; the tests never touch the disk).
  4. The window title, the rail and Sobre show MFPlay with the design's vector
     logo (checked by running the app here).
  5. The repository is `johnnymouraf/mfplay`, with `origin` and the README and
     Sobre links updated.
- **Deliveries:** (1) rename code, CMake, QML module, workflow and docs; (2)
  data migration; (3) `.ico` and logo exported from the design;
  (4) rename the repository and check the app running.

## Auto mode

Scope of the current run: 3.2–3.5, started on 2026-11-24 with the owner at the
machine. The first run (2026-11-22) built 1.1–1.5 and 2.1–2.6. Delivery: each
step works on the branch `auto/<step>` (for example `auto/3.2`), commits and
pushes each delivery there so CI builds it, and the orchestrator fast-forwards
`main` to the branch once the step is done and green.

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
  (`gh run download <id> --name mfplay-windows-x64 --dir <folder>`) and run
  `mfplay.exe`. The log is `%LOCALAPPDATA%\MFPlay\logs\mfplay.log`.
  Screenshots: PowerShell with `System.Drawing` (`CopyFromScreen`) into the
  scratchpad, then read the PNG. Clicking: UI Automation (Qt Quick exposes
  accessible names) or keyboard input through `user32` `SendInput` after
  `SetForegroundWindow`. Always close the app when done.
- **The real console:** the PS5 pairing PIN appears on the console's screen and
  the Account ID belongs to the owner, so pairing, waking and streaming from the
  real console can't be checked unattended. Those criteria are marked "owner
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
- **Design:** the design file under `design/` (on the owner's machine, kept
  out of the repository) is the source of truth for how the app looks. Its
  variables became the QML `Theme` singleton and its components the QML
  components.
- **Stuck:** after three CI attempts at the same failure with no progress, or
  when the step can't fit its run, finish the current delivery, stop and
  report what is left.

### Shared by the redesign steps (3.2–3.5)

- **Reading the design:** the Drafia app is open with the design file; use the
  `mcp__drafia__*` tools (load them with ToolSearch). Read the Drafia skill's
  `guide/design-to-code.md` first. Page "Design system" holds the components
  and foundations; page "Screens" holds the screens in sections 1 Consoles,
  2 Pareamento, 3 Conexão e stream, 4 Configurações. `Inspect` gives exact
  sizes, paddings, gaps, fonts and the variables bound; `TakeScreenshot` the
  visual reference. Never edit the design.
- **Rule for content (settled with the owner):** where the design covers
  something, its layout and copy replace the code's; what exists only in the
  code stays, styled like the design's nearest equivalent (same row, block,
  button and spacing patterns); where the design simplifies a behavior, the
  real behavior stays with the design's look. Nothing working is removed.
- **Owner's visual preferences:** labels start with a capital letter, never
  all-lowercase; solid blocks, no shadows, no decorative filler, no accent
  borders on blocks; every button follows the menu-button pattern (icon in a
  32 px tile + label, 52 px tall), text links become secondary buttons;
  status and error messages are plain colored text (no banner box, no icon);
  no duplicate actions on a screen; status tags (EXPERIMENTAL, NÃO CONECTADA)
  sit next to the row title; no console photos or product art.
- **Checking the look:** run the app from the CI artifact at 1280×800 and
  960×600, screenshot each screen built and compare it with the design frame
  through `mcp__drafia__compare_image` (pass the screenshot's Windows path);
  at most three rounds of fixes per screen, then report what still differs.
- **Interface text** stays Brazilian Portuguese, with "MFPlay" as the name.

### 3.2 Theme and shared components

- **Goal:** the `Theme` singleton holds the design's variables and every shared
  control in `src/app/qml/controls/` looks like its design component, with the
  app shell (rail, hint bar, DualSense card) laid out as the design's screens.
- **Guardrails:** shared controls and the shell only; screens get their own
  layouts in 3.3 and 3.4 (they may only adapt to changed control APIs); no C++
  behavior change beyond what a control needs to show (for example the battery
  value the DualSense card shows).
- **Success criteria:**
  1. Every `Theme` value matches the design's `Theme` collection
     (`GetVariables`), with the design's names; tokens the code needs and the
     design lacks are kept and named for their role. Checked by reading both.
  2. Each control matches its design component in each variant and state:
     Button→`ActionButton`, IconButton, LinkButton (becomes a secondary
     button), StateBadge, ControllerHint/`ControllerHintBar`,
     TextField→`InputField`, Select, PinDigit/PinInput, Toggle,
     SegmentItem/SegmentedControl, Slider→`ValueSlider`, NavItem, AppRail,
     Spinner, ScanIndicator, SettingRow, DialogShell, StatusCount and
     ViewToggle (new), ControllerStatus (new). Checked by screenshots of the
     running app against the design system page.
  3. The shell matches "Consoles — início": rail 232 px with the brand, the
     "MENU" overline, Consoles and Configurações, and Sobre pinned at the
     bottom; the hint bar and the DualSense card along the bottom of the
     content. Sobre opens the existing About content (moved out of the
     settings sections). Checked at 1280×800 and 960×600.
  4. CI green; no QML warnings in the log at startup and while visiting each
     screen.
- **Deliveries:** (1) `Theme` tokens and typography; (2) buttons, badges and
  inputs; (3) toggles, segmented controls, slider, select, PIN; (4) the shell:
  rail with Sobre, hint bar, ControllerStatus, dialogs.
- **Settled in planning:** Banner's tone boxes go away in favor of plain
  colored text where messages appear (the preference above); the rail's
  DualSense block moves into the ControllerStatus card.
- **Changes:** `src/app/qml/Theme.qml`; `src/app/qml/controls/*.qml`; new
  `StatusCount.qml`, `ViewToggle.qml`, `ControllerStatus.qml`;
  `src/app/qml/Main.qml` for the shell; `src/app/CMakeLists.txt` for new QML
  files and Lucide icons the design uses (copied from Lucide, ISC, as the
  existing ones).
- **Data and interfaces:** none beyond QML properties.
- **Edge cases:** no controller connected (the card says so, as the current
  rail does); 960×600 minimum window; keyboard and controller focus rings on
  every focusable control.

### 3.3 Consoles and pairing

- **Goal:** the Consoles screen, its dialogs and the pairing flow match
  sections "1 · Consoles" and "2 · Pareamento" of the design.
- **Guardrails:** consoles and pairing screens only; discovery, wake, pairing
  and store logic unchanged except what the new view mode needs.
- **Success criteria:**
  1. "Consoles — início", "Consoles — vazio", "Consoles — janela mínima",
     "Card do console — estados", "Adicionar pelo IP" and its error match the
     design (compare_image at 1280×800 and 960×600 with consoles added by IP
     to reach each state the machine can produce).
  2. The status counters show the real counts per state; the grid/list toggle
     switches the view and is remembered across restarts (`settings.json`,
     additive field, tested in `SettingsTests`).
  3. Rename and Forget dialogs and the card's "…" menu follow the dialog and
     button patterns.
  4. Pairing steps (Account ID, PIN, pareando, sucesso, erro) match the
     design; the PSN browser login (1.6) stays in step 1, styled like the
     design (secondary button), with manual entry kept; "Ajuda" replaces the
     "Como encontrar meu Account ID" link.
  5. CI green, tests pass, no QML warnings.
- **Deliveries:** (1) cards and grid; (2) counters and grid/list with the
  setting; (3) empty state and Adicionar pelo IP, rename, forget; (4) pairing.
- **Settled in planning:** counters and the grid/list toggle are built now;
  the empty state takes the design's shorter copy and drops the checklist only
  if the design has no place for it (the rule above keeps working hints).
- **Changes:** `src/app/qml/consoles/*.qml`; `src/core/Settings.{h,cpp}` and
  `SettingsController` for the view mode; tests in `tests/core/SettingsTests.cpp`.
- **Data and interfaces:** `settings.json` gains `interface.consoleView`
  (`grid` | `list`, default `grid`), version stays 1.
- **Edge cases:** long console names; many consoles (scrolling keeps the
  focused card visible); a pairing error with retry; the list view with each
  card state.

### 3.4 Settings, Sobre and connecting

- **Goal:** Configurações (Vídeo, Áudio e microfone, Controles, RTX, Rede),
  Sobre and the connecting screens match sections "3 · Conexão e stream"
  (except the overlay) and "4 · Configurações".
- **Guardrails:** these screens only; no streaming behavior change; the Rede
  page's internet rows have no effect yet.
- **Success criteria:**
  1. Each settings page matches its design frame; rows only the code has
     (window mode and size, HDR, upscaler choice, optical flow, controller
     choice, keyboard as controller, mouse sensitivity, invert Y, video test)
     stay, in the design's row style.
  2. Controles shows the DualSense card with the real battery percent and
     charging state from SDL (`SDL_GetGamepadPowerInfo`), and the connection
     type; no controller shows the empty state.
  3. Rede: "Bitrate na rede local" is the same setting as Vídeo's bitrate;
     "Conta PSN" logs in through the existing PSN browser flow and shows the
     state (NÃO CONECTADA tag or the account), its saved Account ID being the
     one pairing fills in; "Bitrate pela internet" and "Endereço fora de casa"
     are shown disabled with a description saying they arrive with internet
     play.
  4. Sobre matches its frame (mark, wordmark, tagline, version chips, the three
     cards) and keeps the video test as an extra card.
  5. "Conectando" and "Conectando — erro" match the design.
  6. CI green, tests pass, no QML warnings.
- **Deliveries:** (1) Vídeo and Áudio; (2) Controles with the battery; (3) RTX
  and Rede; (4) Sobre and connecting.
- **Settled in planning:** RTX keeps the real exclusive upscaler choice
  (Shader / RTX VSR / DLSS SR) shown in the design's style instead of
  independent toggles; Rede is built now with its internet rows disabled.
- **Changes:** `src/app/qml/settings/*.qml` (new `NetworkSettings.qml`),
  `src/app/qml/stream/*.qml`, `InputController` and `GamepadManager` for the
  battery, `PsnLoginClient`/`SettingsController` for the account state.
- **Data and interfaces:** battery as `controllerBatteryPercent` (-1 unknown)
  and `controllerCharging` on `InputController`.
- **Edge cases:** battery unknown (wired, or SDL can't tell); PSN login
  cancelled or failed; no audio devices; no NVIDIA GPU (RTX rows disabled as
  today).

### 3.5 Stream overlay

- **Goal:** the Dear ImGui overlay matches "Overlay no stream (ImGui)": the
  statistics panel (F2) and the Menu rápido (F1) with the design's colors,
  sizes, spacing and controls.
- **Guardrails:** `src/render/Overlay.cpp` and its fonts/colors only; no change
  to what the stats measure or to presentation; the overlay stays drawn by
  Direct3D 12.
- **Success criteria:**
  1. The statistics panel matches the design's panel; every row the current
     panel has stays (Reflex latency, cadence, GPU per stage, the gamepad
     debug section), grouped under the design's overline headings.
  2. Menu rápido matches the design: title with the console name and session
     time, NVIDIA Reflex segmented control, toggles for RTX VSR, Frame
     Generation, Mostrar estatísticas and Microfone, Desconectar as a danger
     button, the close hint at the bottom; the current menu actions (window
     mode row, everything else it has) stay in the same style.
  3. Controller and keyboard navigation of the menu still work.
  4. Checked through the developer file source (`--play-file`) with
     screenshots compared to the design frame; CI green.
- **Deliveries:** (1) shared ImGui style (colors from `Theme`'s values, radii,
  fonts); (2) statistics panel; (3) Menu rápido.
- **Settled in planning:** the Menu rápido is built now.
- **Changes:** `src/render/Overlay.{h,cpp}`, `src/app/OverlayInput.h` if the
  menu model changes.
- **Data and interfaces:** none.
- **Edge cases:** 1080p and 1440p+ outputs (the overlay scales as today);
  HDR output (colors stay correct in HDR10); a game at 30 fps.

### Decisions to review

#### D1 – Scope of the unattended run (all)

The owner asked for everything to be done overnight without interaction, but
pairing and streaming from the real PS5 need the owner (the PIN appears on the
console's screen), and the PSN login (4.2) needs their account.

- A. Up to 1.5 (all of local streaming): stops before the RTX work.
- B. Up to 2.2 (local streaming, Reflex and VSR), every step verifiable here
  through the developer file source.
- C. The whole plan: 4.2 can't be done without the owner's PSN login, and
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

#### D7 – Logo: the MF monogram (design, 3.1)

- A. One vector `AppIcon` (MF monogram over △○✕□, white on the accent) at
  every size, the tray included.
- B. A simpler MF-only mark for the small sizes.

**Chosen: A** (the owner rejected B). The app's `app-icon.svg` and
`mfplay.ico` (16–256 px) are exported from the design's `AppIcon`. Changing
it: edit `AppIcon` in the design and export both again.

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

#### D43 – HDR in "Automático" asks the PS5 for HDR even on an SDR panel (2.3)

- A. Auto is the default and asks for HDR (H.265) whenever the console offers
  it, tone-mapping to SDR here: the native path runs on this machine.
- B. Off by default: nothing changes, but the native stream only runs when
  chosen.
- C. Auto asks only on an HDR display: on this panel Auto equals Off.

**Chosen: A**. Changing it: the default in `core::VideoSettings` and
`videoProfileFor`.

#### D44 – The HDR setting is a three-state row on Vídeo (2.3)

- A. One three-state row (Desligado / Automático / RTX Video HDR) under Vídeo.
- B. Only the sketch's two-state toggle under RTX.
- C. Both: two controls for one setting.

**Chosen: A**. Changing it: add a toggle to `RtxSettings.qml` bound to `hdr`.

#### D45 – Display HDR detection through DisplayConfig (2.3)

The panel belongs to the Intel GPU and the NVIDIA adapter enumerates no
outputs, so `GetContainingOutput` finds nothing.

- A. `DisplayConfigGetDeviceInfo` with `GET_ADVANCED_COLOR_INFO_2` on the
  window's monitor.
- B. Enumerate outputs of every adapter and match the `HMONITOR` (gives
  luminance but not the HDR state).
- C. `GetContainingOutput` (fails here).

**Chosen: A**. Changing it: use B for the panel's luminance.

#### D46 – RTX Video HDR and VSR share the blit's timing (2.3)

Both run in one `VideoProcessorBlt`.

- A. A separate `RtxVideoHdr` stage; when both run, both report the shared
  blit's time.
- B. Attribute the blit to one and show 0 for the other.

**Chosen: A**, each feature shows its real cost and the frame total doesn't
change. Changing it: measure separately if the driver exposes it.

#### D47 – Cadence windows in fixed 60-flag blocks (2.4)

- A. Consecutive 60-flag blocks, published once two agree: real hysteresis,
  settles in ~2 s, simple tests.
- B. A sliding window against the previous one: faster, but 98% overlapping
  and a weak filter.

**Chosen: A**. Changing it: `CadenceDetector::addFlag`/`finishWindow`.

#### D48 – Flow time from a fence-watching list (2.4)

The flow engine runs outside the command queue.

- A. An observer list with GPU timestamps, accurate, waiting for the flow
  inside the 16.7 ms budget.
- B. CPU clock: simple but not `GpuTimer`.
- C. Only the frame total.

**Chosen: A**. Changing it: remove `recordFlowWatch`.

#### D49 – The intermediate is BGRA8 and the flow also runs with VSR (2.4)

NvOF accepts only BGRA8, and VSR replaces the color conversion.

- A. BGRA8 intermediate and an extra ~0.05 ms conversion with VSR.
- B. Disable flow with VSR, which blocks VSR + FG in 2.5/2.6.
- C. Keep RGBA8 and add a second conversion.

**Chosen: A**. Changing it: skip the conversion in
`recordVideoSuperResolutionFrame`.

#### D50 – The panel shows the last source frame, not overlay redraws (2.4)

With the overlay visible, redraws were zeroing the flow and cadence stages.

- A. Keep the last new frame's times per back buffer.
- B. Show every present (flickers to 0.00).
- C. Stop redrawing the overlay.

**Chosen: A**. Changing it: remove `bufferShowedNewFrame_`.

#### D51 – The cadence assumes a 60 fps container (2.4)

The PS5 always sends 60 and `--play-file` doesn't pass its rate.

- A. Fixed at 60 (a different `--fps` would read wrong).
- B. Carry the rate through `FrameRenderer::Options`.

**Chosen: A**. Changing it: transport the rate.

#### D52 – In HDR the flow reads a BGRA8 copy of the 10-bit intermediate (merge 2.3 + 2.4)

NvOF needs BGRA8 while PQ streams use a 10-bit intermediate.

- A. One presenter with two intermediates (SDR BGRA8, PQ RGBA10) and, in HDR,
  a per-buffer BGRA8 copy produced by a scale pass (tone-mapped for PQ),
  written only when flow is on; the cadence shader reads the decoded luma
  plane (R16 for P010, R8 for NV12).
- B. Run the flow on the decoded frame or the swap chain.

**Chosen: A**. Changing it: `FramePresenter`'s intermediate paths.

#### D53 – A missing `HwSchMode` counts as on (2.5)

Windows 11 may leave the value absent at the default.

- A. Absent means off, which would disable FG here.
- B. Assume on and let `FrameGeneration.Available` decide, the runtime's word
  being final; `HwSchMode = 1` still disables with an explanation.

**Chosen: B**. Changing it: `FrameGeneration::probe`.

#### D54 – Motion-vector scale `-1/W, -1/H` (2.5)

The DLSS-G header says "normalize", the guide says "pixels".

- A. `1/W`.
- B. `1`.
- C. `-1/W`, as Veyra does with current→previous pixel vectors.

**Chosen: C**; the sign still needs a real pan to confirm. Changing it:
`kMotionVectorScaleSign`.

#### D55 – The 30 fps schedule holds the real frame for the repeat (2.5)

- A. Present generated and real on the distinct frame and the repeat too
  (~90 fps).
- B. Hold the real frame for the repeat: two presents per pair, 60 from 30.

**Chosen: B**, unit-tested. Changing it: `FrameSchedule::observe`/`presentReal`.

#### D56 – A source ring apart from the swap chain (2.5)

With FG the swap chain advances twice per source frame, so slots would
misalign.

- A. Index by the swap chain buffer.
- B. A ring of three source slots.

**Chosen: B**. Changing it: `sourceSlot_`/`renderFrame`.

#### D57 – One frame-slot wait per present (2.5)

- A. Two waits per source frame.
- B. One wait per present (the loop's wait grants the first).

**Chosen: B**; it worked as is: 30→60 and 60→120 with no steady-state drops
and `SetMaximumFrameLatency(1)` intact. Changing it: a cadence/Streamline
pacer if the owner sees stutter.

#### D58 – A segmented control for Frame Generation (2.5)

- A. The design's toggle.
- B. A two-state segmented control (Desligado / Ligado 2x), as the criterion
  names.

**Chosen: B**. Changing it: the design's toggle in `RtxSettings.qml`.

#### D59 – A warm-up DLSS-G evaluate on creation (2.5)

The runtime allocates internals on the first evaluate.

- A. Let the first real frame pay for it.
- B. A reset evaluate at creation, discarded if it fails.

**Chosen: B**. Changing it: `prepareFrameGeneration`.

#### D60 – One NGX host after the merge (2.6)

Both steps built in parallel and each created a `NgxHost`.

- A. Each step keeps its own host.
- B. One host, created on first need by the renderer, owning the device,
  `Init_with_ProjectID`, the log callback, the `%LOCALAPPDATA%` data and the
  `PathListInfo` that lets the loader find the DLLs beside the exe; both
  features are created against it and released before it.

**Chosen: B** (each step built with its own during the parallel run).
Changing it: split the host per feature again.

#### D61 – The upscaler defaults to the shader, with VSR as fallback (2.6)

- A. Shader by default; whenever DLSS can't run, VSR draws the stream.
- B. VSR by default, paying its cost for everyone.
- C. DLSS by default, despite being experimental.

**Chosen: A**, keeping 2.2's opt-in cost. Changing it:
`RtxSettings::upscaling` and the screen's text.

#### D62 – DLSS depth through an upload-buffer copy (2.6)

A `R32_FLOAT` depth-stencil view made `CreateFeature` fail.

- A. A DSV in `R32_FLOAT` (failed).
- B. An upload buffer copied into the resource (chosen).
- C. A compute shader, another PSO.

**Chosen: B**. Changing it: a DSV again only after finding the invalid call.

#### D63 – On an HDR10 swap chain, DLSS falls back to VSR (2.6)

The feature is 8-bit; writing SDR into an HDR10 chain would present it as PQ.

- A. An SDR→PQ pass after DLSS.
- B. Fall back to VSR on an HDR10 chain; a PQ stream runs DLSS on the
  tone-mapped SDR copy.

**Chosen: B**. Changing it: the SDR→PQ pass.

#### D64 – An Ada gate through NVAPI (2.6)

- A. Let NGX fail.
- B. Allow every RTX.
- C. `NvAPI_GPU_GetArchInfo >= AD100`, reading the architecture without
  creating a device, with the reason on screen.

**Chosen: C**. Changing it: `dlssArchitectureSupported`/`probeDlss`.

#### D65 – Redraws reuse the slot's flow (2.6)

With the overlay open, frames redraw continuously.

- A. Switch to VSR on redraws.
- B. Re-run the flow on every redraw.
- C. Reuse the last flow of the slot (same source frame).

**Chosen: C**. Changing it: require `plan.opticalFlow` again.

#### D66 – One selector replaces the VSR row (2.6)

- A. Three toggles.
- B. One selector (Shader / VSR / DLSS) with the design's "Experimental"
  badge.

**Chosen: B**, matching `rtx.upscaling`. Changing it: separate rows.


#### D67 – PSN tokens live in their own sealed file (1.6)

The console store already has the DPAPI scheme, but adding a second writer
there (or in `settings.json`) would mix subjects.

- A. A `psn.json` in `%APPDATA%\MFPlay` sealed with the same
  `SecretProtector`, leaving `consoles.json` and `settings.json` untouched.
- B. Extend `consoles.json`: two writers.
- C. A sealed blob inside `settings.json`: violates "settings keeps only the
  expiry and the flag".

**Chosen: A**. Changing it: swap the store in `main()`/`PsnLoginClient`, the
UI untouched.

#### D68 – The login is a property on `PairingController` (1.6)

- A. A `PsnLoginClient` created in `main`, exposed as a `Q_PROPERTY` on
  `PairingController`, which the dialog already reaches.
- B. Create it inside `ConsolesController`: mixes account and network there.
- C. Plumb it through the QML pages.

**Chosen: A**. Changing it: move the property and the constructor argument.

#### D69 – Refresh the expired access token at startup (1.6)

- A. Renew silently in the background when the saved access token expired,
  with a guard against stale responses.
- B. Wait for 3.2 to ask.
- C. Never renew.

**Chosen: A**, closing the edge case now. Changing it: drop the call in
`PsnLoginClient`'s constructor.


#### D70 – The overlay's mutex became recursive (1.7)

`ReleaseCapture` makes Windows re-enter the window procedure synchronously,
and the overlay locked its own non-recursive mutex again.

- A. A `std::recursive_mutex`: keeps cross-thread exclusion and delivers
  every message.
- B. Ignore nested messages: drops input.
- C. Marshal messages to the render thread: more machinery.

**Chosen: A**. Changing it: the type in `Overlay.cpp`.

#### D71 – The pipeline goes idle while the window can't show it (1.7)

- A. Skip drawing and presenting while the window is iconic or has a zero
  client area, resuming on restore and restarting the DLSS-G/cadence pairs;
  an occluded chain retries periodically.
- B. Keep presenting: GPU work for an unseen picture.
- C. Skip only the present: the pipeline still runs.

**Chosen: A**. Changing it: the `PresentGate` use in `renderFrame`.

#### D72 – `IsIconic` in addition to the client rect (1.7)

- A. Treat a window as hidden when `IsIconic` says so or the client area is
  zero.
- B. Only the client rect: a minimized window was seen presenting once.

**Chosen: A**. Changing it: `clientAreaOf`.


#### D73 – The optical-flow teardown runs on the render thread (1.8)

The fault is the driver releasing the presenter textures the NvOF read, not
`nvOFDestroy` itself.

- A. Release the session and the presenter pictures on the render thread
  after the GPU is idle, each driver call under an SEH guard.
- B. Guard only the current release on the main thread: survives, but off the
  thread that used the driver.
- C. Guard only the NvOF calls: insufficient, the fault comes later.

**Chosen: A**. Changing it: remove `presenter_.releasePictures()` from the
release path.

#### D74 – One guard per driver call, not one around the block (1.8)

A fault then doesn't skip the remaining releases, and the log names the
resource; the cost is a `[critical]` line per picture.

- A. One guard per call.
- B. A single guard around everything: one fault leaks the rest.

**Chosen: A**. Changing it: a single guard.

#### D75 – The release order is frozen in an enum with tests (1.8)

The CI runner has no GPU.

- A. An enum plus a runner test covering the order and the continue-after-fault
  rule.
- B. No test: the order depends on reading code.

**Chosen: A**. Changing it: remove the enum and the test.

#### D76 – The owner check ran here (1.8)

The PS5 was on the network and the saved console opened real sessions.

- A. Ten real open/close cycles here, plus the file loops.
- B. Leave it as an owner check.

**Chosen: A**; the owner still confirms in their own session. Changing it:
reclassify as an owner check.



#### D82 – The F1 window row cycles instead of opening a submenu (1.10)

The menu has no dropdown and is driven by mouse and D-pad.

- A. A row that cycles fullscreen and the presets, in the existing rows'
  pattern; leaving fullscreen restores the remembered size (or the first
  preset when the size is custom, so no preset is unreachable).
- B. A new submenu with its own state.

**Chosen: A**. Changing it: `MenuResult`/`drawMenu`.

#### D83 – A 500 ms debounce for resizes (1.10)

`SetWindowPos` doesn't send `WM_EXITSIZEMOVE`.

- A. Only `WM_EXITSIZEMOVE`: misses programmatic sizes.
- B. A 500 ms timer on `WM_SIZE` plus an immediate report at the end of a
  user drag.

**Chosen: B**. Changing it: `kSizeSettleMilliseconds`.

#### D84 – The cursor through `SetCursor`, not `ShowCursor` (1.10)

`ShowCursor` is a per-thread counter, and the old code hid the cursor in
every mode.

- A. A balanced `ShowCursor` counter.
- B. `SetCursor(nullptr)` with `WM_SETCURSOR`, showing the arrow in windowed
  mode and hiding after two idle seconds in fullscreen, keeping it visible
  with the F1 menu.

**Chosen: B**. Changing it: `applyCursor()`/`kCursorHideMilliseconds`.

#### D85 – F11 persists the mode (1.10)

- A. Keep it in memory only.
- B. An `onWindowModeChanged` callback stores it, so a new stream opens the
  same way.

**Chosen: B**. Changing it: remove the callback wiring.

#### D86 – A two-pixel tolerance for the presets (1.10)

A drag lands one or two pixels off the round sizes.

- A. Exact matching: a 1280×721 window shows "Personalizado".
- B. Two pixels per dimension, storing the real client size.

**Chosen: B**. Changing it: `kWindowSizePresetTolerance`.

#### D87 – The video test follows the same layout (1.10)

It is the only path that runs a stream window with the settings UI alive,
used in this step's live checks.

- A. Apply the same layout there too, without touching the video profile.
- B. Leave it fixed at its own size.

**Chosen: A**. Changing it: `AppController`.
#### D88 – The game rate counts cadence decisions in the same 2 s window (1.11)

- A. Count the distinct decisions in the metrics' 2 s window.
- B. Use the detector's published estimate: safer on a static scene but slow
  after a cut and a second cadence line.

**Chosen: A**. Changing it: `StreamMetrics::addCadenceFrame`/`gameFps`.

#### D89 – "Exibidos" means new pictures (1.11)

- A. Distinct plus generated.
- B. Every Present call: counts repeats and shows 60 in a 30 fps game with
  FG off.

**Chosen: A**, matching the criterion that FG off shows generated 0 and
displayed equal to the game. Changing it: `StreamMetrics::snapshot`.

#### D90 – Repeated presents are derived, not counted separately (1.11)

- A. Count real presents of new frames and derive repeats as real minus game.
- B. Count every Present, including overlay redraws.

**Chosen: A**. Changing it: `addPresented(..., newFrame)`.

#### D91 – The cadence ignores the HUD band (1.11)

A 60 fps HUD over a 30 fps picture made every frame differ.

- A. Count only the central 3/4 of each axis in the cadence shader.
- B. Raise the repeat threshold: a large HUD still crosses it.
- C. Average the whole picture: same problem.

**Chosen: A**. Changing it: the `/8` margin in `Cadence.hlsl`.

#### D92 – The metrics window is the panel's 2 s (1.11)

- A. 2 s, the window the panel already used.
- B. 1 s, as Veyra's rate windows do.

**Chosen: A**. Changing it: `kMetricsWindow`.
