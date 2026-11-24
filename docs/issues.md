# Issues

## Discovery gets no answers until the firewall allows the app

- **Where:** console discovery (`src/remoteplay/ConsoleDiscovery.cpp`) on a
  network Windows marks as Public.
- **Steps:** extract a build to a new folder, run `mfplay.exe`, dismiss or
  ignore the Windows Firewall prompt, wait on the Consoles screen with a PS5 on
  the LAN.
- **Expected:** the PS5 appears, or the app explains that the firewall is
  blocking discovery and how to allow it.
- **Actual:** Windows adds inbound block rules for that `mfplay.exe` path
  (Public profile), broadcast answers never arrive and the list stays empty
  with no explanation. Direct queries to a typed IP still work. Each new
  extraction path triggers a new prompt.

## A focused console card is cut off after shrinking the window

- **Where:** `src/app/qml/ConsolesPage.qml`.
- **Steps:** add three consoles by IP, move the focus to the third card,
  resize the window to 960×600.
- **Expected:** the grid scrolls so the focused card stays fully visible.
- **Actual:** the card stays cut off at the bottom until the focus moves.

## Direct discovery query drops the card when the console answers on one port only

- **Where:** `src/remoteplay/ConsoleDiscovery.cpp`, the direct query added by
  "Adicionar pelo IP".
- **Steps:** add a console by IP whose host answers the PS5 discovery port
  (UDP 9302) but not the PS4 one (UDP 987), while discovery runs.
- **Expected:** the card shows the answer from 9302.
- **Actual:** the ICMP port-unreachable from 987 surfaces as `WSAECONNRESET`,
  the log says "Discovery thread failed to read from socket", discovery stops
  and the card stays offline.

## A paired but switched-off DualSense shows up as a gamepad for half a second

- **Where:** `GamepadManager::openAll` / event handling at startup.
- **Steps:** keep a paired DualSense switched off and start the app.
- **Expected:** no gamepad listed.
- **Actual:** occasionally one gamepad is enumerated and removed about 0.5 s
  later (`SDL reports 1 gamepad(s)` then `Gamepad disconnected`); later runs
  report none. Seen once; likely SDL/Windows behavior with a paired HID device.

## A paired but sleeping DualSense isn't listed by SDL while Windows lists it

- **Where:** `GamepadManager::openAll` at startup.
- **Steps:** leave a paired DualSense asleep (not connected to the PC) and
  start the app; compare with the Windows game controllers list.
- **Expected:** either the controller is listed as connected, or it isn't
  listed in both places.
- **Actual:** Windows lists the paired controller and SDL reports 0 gamepads,
  so the rail shows "Desconectado" and the controller can't be used until it
  wakes.

## Parallel test runs share the owner's settings and logs

- **Where:** any two `mfplay.exe` instances started from different builds
  (the step agents' tests).
- **Steps:** run two builds at the same time; change settings in one.
- **Expected:** each run is isolated.
- **Actual:** every build reads `%APPDATA%\MFPlay\settings.json` and writes
  `%LOCALAPPDATA%\MFPlay\mfplay.log`, so one run overwrites the other's
  settings (fields disappear) and the log lines interleave. Reproducible; a
  verification hazard, not a runtime bug.

## Reflex reports no effective low-latency in --play-file

- **Where:** `--play-file` playback with Reflex on.
- **Steps:** run `mfplay.exe --play-file <file> --fps 60`, open F2, read the
  Reflex section.
- **Expected:** the driver reports the low-latency mode as effective, and
  `GetLatency` returns a valid report, as the "Teste de vídeo" window does.
- **Actual:** `NvAPI_D3D_GetSleepStatus` returns `bLowLatencyMode = 0` and
  the latency report is invalid, although `SetSleepMode` succeeded, the
  driver sees the sleep and both kinds of markers are accepted. The live test
  window shows both valid.

## The first DLSS feature creation stalls for about two seconds

- **Where:** `DlssSuperResolution::create` (and Frame Generation) on the
  render thread.
- **Steps:** start `--play-file` with `rtx.upscaling: "dlss"` and watch the
  overlay for the first five seconds.
- **Expected:** the rate holds near 60 fps while the feature is created.
- **Actual:** `CreateFeature` blocks the render thread for ~1.8–2 s and the
  first seconds show ~37–39 fps. The research suggests creating the feature
  off the present thread.

## The NVIDIA driver still faults releasing pictures the optical flow read

- **Where:** closing the stream window (`--play-file` or a real session) with
  `rtx.opticalFlow` on.
- **Steps:** play with the optical flow enabled and press Esc.
- **Expected:** the app exits with the normal shutdown lines and code 0.
- **Actual:** the driver faults with `0xC0000005` in `nvwgf2umx.dll`
  (offsets `0xd2cd86`/`0xd8d217`) when the last reference to the pictures the
  flow read is released. Since step 1.8 the release runs on the render thread
  behind a guard: the app survives, and each picture logs one `[critical]`
  line. Seen on 2026-11-22 and 2026-11-23; a driver update may remove it.
