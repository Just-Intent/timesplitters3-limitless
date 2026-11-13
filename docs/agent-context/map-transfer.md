# MapMaker map transfer (.tsmap)

Player guide: [MapMaker map export and import](../players/mapmaker-map-transfer.md).

Maps are XAPI save directories `UserData/45410066/<12 hex>` beside profiles, not
inside them: every profile of a save folder shares them. The name is the lifted
`sub_0037DCE0` hash (`h*0x10000+c mod 0xFFFFFFFFFFC5` over the UTF-16 save name
`MapMaker: <name>`, French `Carte: `). Files: `SaveMeta.xbx` (UTF-16LE
`Name=...`) and `anicemap.mkr` (0x7FE0-byte body + zero-key HMAC-SHA1, as in
`tools/testing/testing_profile.py`). File offsets: pieces +0x14, tileset +0x1A,
name +0x72, used size +0x258; zlib CRC-32 of the first `used` bytes at `used`.

Entry points: `tools/launcher/MapTransfer.cs` (List, Parse, Pack/Unpack, Rename,
Export, ImportInto, Import); the MapMaker maps section of `SavesPage.cs`; file
pickers in `Launcher.cs` (WPF dialogs) and `macos/NativeWindow.cs` (Avalonia
StorageProvider, also used by Linux). Export is read-only for saves and refuses
targets inside the save folder. Import holds the save lock (reusing the window's
ownership), refuses while the game runs, calls `SaveBackups.CreateLocked` with
reason `Before map import` (accepted by the manifest check), never writes
existing files and stages in `.tsmap-import-<guid>` before one directory move.

Stock limits enforced by Parse: one 32 KB body size (`Tiers = {0x7FE0}`; larger
bodies are refused with an explicit message), at most 200 pieces, tileset < 7,
names 1-31 printable ASCII. `.tsmap` reads are bounded to 1 MiB.

Tests: `tests/launcher/launcher_maps.cs` (run from the shared UI checks) and a
Saves page check in `launcher_shared_ui.cs`; compiled by `build-launcher.ps1`,
`NativeLauncher.csproj` and `LinuxLauncher.csproj`. Mac/Linux host runs and
in-game loading of an imported map are unverified (MAC-131). Older launchers
list a `Before map import` backup as needing attention.
