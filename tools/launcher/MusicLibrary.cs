using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace TimeSplitters.Launcher
{
    public sealed class MusicTrack
    {
        public string Name;
        public long Bytes;
        public override string ToString()
        {
            return Name + " (" + (Bytes / 1048576.0).ToString("0.0") + " MiB)";
        }
    }

    // Imported music for MapMaker and Arcade (docs/mapmaker/mapmaker-custom-music.md):
    // the game lists the WAV and MP3 files in LauncherPlatform.Music (TSFP_MUSIC_DIR)
    // whose names pass pc_music_name_ok (src/interface/pc_custom_music.h). The rules
    // here mirror that check so every imported copy is one the game will offer.
    public static class MusicLibrary
    {
        public static readonly string[] Extensions = { ".wav", ".mp3" };
        public const int MaxTracks = 64, MaxNameBytes = 63;           // PC_MUSIC_MAX_TRACKS, PC_MUSIC_NAME_MAX - 1
        public const long MaxBytes = 512L * 1024 * 1024;              // one track
        public const string PickerPatterns = "*.wav;*.mp3";
        static readonly Regex Reserved = new Regex(@"\A(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])\z", RegexOptions.IgnoreCase);
        static readonly Regex Staging = new Regex(@"\A\.music-import-[0-9a-f]{32}\.tmp\z");

        public static string Folder(string root) { return Core.PathIn(root, LauncherPlatform.Music(root)); }

        public static bool Supported(string path)
        {
            string extension = Path.GetExtension(path ?? "");
            return Extensions.Any(e => e.Equals(extension, StringComparison.OrdinalIgnoreCase));
        }

        // The game's pc_music_name_ok: a supported extension, no folder, printable,
        // not hidden, 5..63 UTF-8 bytes.
        public static bool GameName(string name)
        {
            if (string.IsNullOrEmpty(name) || name[0] == '.' || !Supported(name)) return false;
            int bytes = Encoding.UTF8.GetByteCount(name);
            if (bytes < 5 || bytes > MaxNameBytes) return false;
            return name.All(c => c >= 0x20 && c != 0x7F && c != '/' && c != '\\' && c != ':');
        }

        // The tracks the game will offer, in its own name order.
        public static MusicTrack[] List(string root)
        {
            string folder = Folder(root);
            if (!Directory.Exists(folder)) return new MusicTrack[0];
            return Directory.GetFiles(folder).Select(f => new FileInfo(f))
                .Where(f => GameName(f.Name) && (f.Attributes & FileAttributes.ReparsePoint) == 0)
                .OrderBy(f => f.Name, StringComparer.OrdinalIgnoreCase)
                .Select(f => new MusicTrack { Name = f.Name, Bytes = f.Length }).ToArray();
        }

        // A file name the game accepts, made from the chosen file's name.
        public static string SafeName(string source)
        {
            string extension = Path.GetExtension(source).ToLowerInvariant(), stem = Path.GetFileNameWithoutExtension(source);
            var text = new StringBuilder();
            foreach (char c in stem)
                text.Append((c < 0x80 && (char.IsLetterOrDigit(c) || " -_()&',!+".IndexOf(c) >= 0)) ? c : '_');
            string result = Regex.Replace(text.ToString(), "_{2,}", "_").Trim(' ', '.', '_');
            if (result.Length == 0) result = "Track";
            if (Reserved.IsMatch(result)) result = "_" + result;
            int room = MaxNameBytes - extension.Length - 4;            // " (n)" may follow
            if (result.Length > room) result = result.Substring(0, room).TrimEnd(' ', '.');
            return result + extension;
        }

        static string Unique(string folder, string name, HashSet<string> taken)
        {
            string stem = Path.GetFileNameWithoutExtension(name), extension = Path.GetExtension(name);
            for (int n = 1; n < 1000; n++)
            {
                string candidate = n == 1 ? name : stem + " (" + n + ")" + extension;
                if (!taken.Contains(candidate) && !File.Exists(Path.Combine(folder, candidate)) && !Directory.Exists(Path.Combine(folder, candidate))) return candidate;
            }
            throw new InvalidDataException("No free name for " + name + "; rename or remove some tracks first.");
        }

        // Refuses files that are not what their extension says: a RIFF/WAVE header, or
        // an MP3's ID3v2 tag or MPEG audio frame sync. The game checks the audio itself.
        public static void CheckFile(string path)
        {
            Core.Require(Supported(path), Path.GetFileName(path) + " is not a WAV or MP3 file.");
            Core.Require(File.Exists(path), Path.GetFileName(path) + " was not found.");
            Core.RequireUnlinked(path, "Linked music files are not supported.");
            long length = new FileInfo(path).Length;
            Core.Require(length > 0, Path.GetFileName(path) + " is empty.");
            Core.Require(length <= MaxBytes, Path.GetFileName(path) + " is larger than " + (MaxBytes >> 20) + " MiB.");
            var head = new byte[12];
            int got;
            using (var input = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read)) got = input.Read(head, 0, head.Length);
            bool wav = Path.GetExtension(path).Equals(".wav", StringComparison.OrdinalIgnoreCase);
            if (wav)
                Core.Require(got == 12 && Encoding.ASCII.GetString(head, 0, 4) == "RIFF" && Encoding.ASCII.GetString(head, 8, 4) == "WAVE",
                    Path.GetFileName(path) + " is not a WAV file.");
            else
                Core.Require(got >= 3 && (Encoding.ASCII.GetString(head, 0, 3) == "ID3" || (head[0] == 0xFF && (head[1] & 0xE0) == 0xE0)),
                    Path.GetFileName(path) + " is not an MP3 file.");
        }

        static void RemoveStaging(string folder)
        {
            foreach (string file in Directory.GetFiles(folder))
                if (Staging.IsMatch(Path.GetFileName(file))) { try { File.Delete(file); } catch (IOException) { } catch (UnauthorizedAccessException) { } }
        }

        // Copies the chosen files into the music folder under free game-safe names.
        // Each copy is written to a staging file, flushed and then moved into place, so
        // an interrupted import never leaves a partial track; nothing is replaced.
        public static MusicTrack[] Import(string root, string[] sources, IProgress<string> progress, CancellationToken token, bool installationLockHeld = false)
        {
            Core.Require(sources != null && sources.Length > 0, "Choose at least one WAV or MP3 file.");
            foreach (string source in sources) CheckFile(source);
            Directory.CreateDirectory(Core.PathIn(root, ".launcher"));
            using (LauncherPlatform.AcquireSaveLock(root, installationLockHeld))
            {
                string folder = Folder(root);
                Directory.CreateDirectory(folder);
                Core.RequireUnlinked(folder, "A linked music folder is not supported.");
                RemoveStaging(folder);
                var existing = List(root);
                Core.Require(existing.Length + sources.Length <= MaxTracks,
                    "The game lists up to " + MaxTracks + " tracks; " + existing.Length + " are already imported.");
                long need = sources.Sum(s => new FileInfo(s).Length);
                Core.Require(new DriveInfo(Path.GetPathRoot(Path.GetFullPath(folder))).AvailableFreeSpace > need + (64L << 20),
                    "There is not enough free space to copy this music.");
                string full = Path.GetFullPath(folder).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
                // The game matches names without case, so a case-only twin counts as taken.
                var taken = new HashSet<string>(Directory.GetFileSystemEntries(folder).Select(Path.GetFileName), StringComparer.OrdinalIgnoreCase);
                var added = new List<MusicTrack>();
                var buffer = new byte[1 << 20];
                foreach (string source in sources)
                {
                    token.ThrowIfCancellationRequested();
                    Core.Require(!Path.GetFullPath(source).StartsWith(full, SaveLocations.Comparison), Path.GetFileName(source) + " is already in the music folder.");
                    string name = Unique(folder, SafeName(source), taken);
                    string staging = Path.Combine(folder, ".music-import-" + Guid.NewGuid().ToString("N") + ".tmp");
                    if (progress != null) progress.Report("Importing " + Path.GetFileName(source) + "...");
                    try
                    {
                        using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.Read))
                        using (var output = new FileStream(staging, FileMode.CreateNew, FileAccess.Write))
                        {
                            long copied = 0, total = input.Length;
                            for (int read; (read = input.Read(buffer, 0, buffer.Length)) > 0; )
                            {
                                token.ThrowIfCancellationRequested();
                                copied += read;
                                Core.Require(copied <= MaxBytes, Path.GetFileName(source) + " grew past " + (MaxBytes >> 20) + " MiB while it was copied.");
                                output.Write(buffer, 0, read);
                            }
                            Core.Require(copied == total, Path.GetFileName(source) + " changed while it was copied.");
                            output.Flush(true);
                        }
                        // File.Move never replaces: a track that appeared meanwhile is kept.
                        File.Move(staging, Path.Combine(folder, name));
                    }
                    finally { if (File.Exists(staging)) File.Delete(staging); }
                    taken.Add(name);
                    added.Add(new MusicTrack { Name = name, Bytes = new FileInfo(Path.Combine(folder, name)).Length });
                }
                return added.ToArray();
            }
        }

        // Hands one listed track to the host's removal (recycle bin / Trash).
        public static void Remove(string root, string name, Action<string> recycle, bool installationLockHeld = false)
        {
            Core.Require(recycle != null, "Removing music is not available here.");
            Core.Require(GameName(name) && Path.GetFileName(name) == name, "That is not an imported track.");
            Directory.CreateDirectory(Core.PathIn(root, ".launcher"));
            using (LauncherPlatform.AcquireSaveLock(root, installationLockHeld))
            {
                Core.Require(!Core.GameRunning(), "Close the game before removing music.");
                Core.Require(List(root).Any(t => t.Name == name), name + " is no longer in the music folder.");
                string path = Path.Combine(Folder(root), name);
                Core.RequireUnlinked(path, "Linked music files are not supported.");
                recycle(path);
                Core.Require(!File.Exists(path), name + " could not be removed.");
            }
        }
    }
}
