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
