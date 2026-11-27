# MapMaker map transfer (.tsmap)

Owner doc: [mapmaker-map-transfer.md](../mapmaker/mapmaker-map-transfer.md).

Maps are XAPI save directories `UserData/45410066/<hash>` beside profiles, not
inside them: every profile of a save folder shares them. The hash is the lifted
`sub_0037DCE0` (`h*0x10000+c mod 0xFFFFFFFFFFC5` over the UTF-16 name
`MapMaker: <name>`). Files: `SaveMeta.xbx` and `anicemap.mkr` (tier body +
zero-key HMAC-SHA1). Body name +0x72, pieces +0x14, used +0x258, CRC at used.

Entry points: `tools/launcher/MapTransfer.cs` (List, Parse, Pack/Unpack,
Rename, Export, ImportInto, Import); Saves page section in `SavesPage.cs`;
pickers in `Launcher.cs` (WPF dialogs) and `macos/NativeWindow.cs` (Avalonia
StorageProvider). Import holds the save lock, refuses while the game runs,
calls `SaveBackups.CreateLocked` (reason `Before map import`, now accepted by
the manifest check), never writes existing files and stages in
`.tsmap-import-<guid>`. Caps: 1023 pieces (the game cap since .55), tileset < 7, five size tiers.

Tests: `tests/launcher/launcher_maps.cs` via shared UI checks, plus a page
test in `launcher_shared_ui.cs`; both builds compile them (build-launcher.ps1,
NativeLauncher.csproj). Windows passed 2026-11-06; Mac and in-game loading of
an imported map are unverified. Older launchers list a `Before map import`
backup as needing attention.
