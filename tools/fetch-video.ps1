# The runtime compiles against these headers and loads the DLLs at runtime.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $root 'external\ts3FPrecomp\third_party\ffmpeg'
if (Test-Path -LiteralPath (Join-Path $destination 'include\libavcodec\avcodec.h')) { return }
if (Test-Path -LiteralPath $destination) { throw 'FFmpeg destination exists but is incomplete; inspect it first.' }
$downloadRoot = Join-Path $root 'private\downloads'
New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
$archive = Join-Path $downloadRoot 'ffmpeg.zip'
# Month-end BtbN builds are retained permanently; the moving 'latest' asset is
# rebuilt daily and broke the earlier pin (re-pinned 2026-10-31).
$url = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-09-30-13-08/ffmpeg-n8.1.3-9-g29e619e767-win64-lgpl-shared-8.1.zip'
$expected = '3E47BDA1607740550141E37C0E49D1E5182B34699F15ADFD137EE266D346811A'
if (!(Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $url -OutFile $archive }
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
    throw 'FFmpeg archive differs from the validated build. Review the updated release before changing the pinned checksum.'
}
$unpackRoot = Join-Path $downloadRoot 'ffmpeg'
Expand-Archive -LiteralPath $archive -DestinationPath $unpackRoot -Force
$unpacked = @(Get-ChildItem -LiteralPath $unpackRoot -Directory)
if ($unpacked.Count -ne 1) { throw 'Unexpected FFmpeg archive layout.' }
$sourcePath = [IO.Path]::GetFullPath($unpacked[0].FullName)
$allowedRoot = [IO.Path]::GetFullPath($unpackRoot) + [IO.Path]::DirectorySeparatorChar
if (!$sourcePath.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'FFmpeg source is outside the download directory.' }
Move-Item -LiteralPath $sourcePath -Destination $destination
Write-Output 'FFmpeg 8.1 headers and shared libraries are ready.'
