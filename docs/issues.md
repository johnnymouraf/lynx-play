# Issues

## Discovery gets no answers until the firewall allows the app

- **Where:** console discovery (`src/remoteplay/ConsoleDiscovery.cpp`) on a
  network Windows marks as Public.
- **Steps:** extract a build to a new folder, run `lynx-play.exe`, dismiss or
  ignore the Windows Firewall prompt, wait on the Consoles screen with a PS5 on
  the LAN.
- **Expected:** the PS5 appears, or the app explains that the firewall is
  blocking discovery and how to allow it.
- **Actual:** Windows adds inbound block rules for that `lynx-play.exe` path
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

- **Where:** any two `lynx-play.exe` instances started from different builds
  (the step agents' tests).
- **Steps:** run two builds at the same time; change settings in one.
- **Expected:** each run is isolated.
- **Actual:** every build reads `%APPDATA%\LynxPlay\settings.json` and writes
  `%LOCALAPPDATA%\LynxPlay\lynx-play.log`, so one run overwrites the other's
  settings (fields disappear) and the log lines interleave. Reproducible; a
  verification hazard, not a runtime bug.
