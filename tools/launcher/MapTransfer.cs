using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace TimeSplitters.Launcher
{
    // One saved MapMaker map: an XAPI save directory UserData/45410066/<12 hex>
    // holding SaveMeta.xbx ("Name=MapMaker: <name>") and anicemap.mkr (body + 20-byte
    // HMAC-SHA1 signature). Maps belong to the save folder, not to a game profile.
    public sealed class MapEntry
    {
        public string Folder, Name, Prefix, Problem;
        public int BodySize, Used, Pieces, Tileset;
        public bool SignatureMatches;
        public bool Valid { get { return Problem == null; } }
        public string Tier { get { return (BodySize + 32) / 1024 + " KB"; } }
        public override string ToString()
        {
            if (!Valid) return (Name ?? Folder) + " (cannot be exported: " + Problem + ")";
            return Name + " - " + Pieces + " pieces, " + Tier + " file";
        }
    }

    public sealed class MapPackage
    {
        public Dictionary<string, object> Metadata;
        public byte[] File;
        public MapEntry Map;
    }

    // Portable .tsmap files (docs/mapmaker/mapmaker-map-transfer.md):
    //   "TSFPMAP1", u32 header length, UTF-8 JSON metadata, u32 map length,
    //   the exact anicemap.mkr bytes, SHA-256 of everything before it.
    public static class MapTransfer
    {
        public const string TitleFolder = "UserData/45410066", MapFile = "anicemap.mkr", MetaFile = "SaveMeta.xbx";
        public const string ExportFolder = "Backups/Maps", Extension = ".tsmap", BackupReason = "Before map import";
        public const int SignatureSize = 20, MaxPieces = 511, MaxNameBytes = 31, MaxTileset = 7;
        public static readonly int[] Tiers = { 0x7FE0, 0xFFE0, 0x1FFE0, 0x3FFE0, 0x7FFE0 };
        public static readonly string[] Prefixes = { "MapMaker: ", "Carte: " };
        const uint Magic = 0x00C77667, Version = 0x12D, InfoTag = 0x00C77668, EndTag = 0x00C7766A;
        const int InfoData = 16, UsedOffset = InfoData + 0x248, PiecesOffset = InfoData + 4, TilesetOffset = InfoData + 0xA;
        const int NameOffset = InfoData + 0x62, NameField = 32, MaxHeader = 65536;
        static readonly byte[] PackageMagic = Encoding.ASCII.GetBytes("TSFPMAP1");
        static readonly Regex SaveDirectory = new Regex(@"\A[0-9A-F]{12}\z"), Staging = new Regex(@"\A\.tsmap-import-[0-9a-f]{32}\z");

        // XAPI save directory name (lifted sub_0037DCE0): h = (h * 0x10000 + c) mod
        // 0xFFFFFFFFFFC5 over the UTF-16 save name, printed as 12 uppercase hex digits.
        public static string DirectoryName(string saveName)
        {
            ulong h = 0;
            // h < 2^48, so h * 0x10000 + c stays below 2^64.
            foreach (char c in saveName) h = (h * 0x10000UL + c) % 0xFFFFFFFFFFC5UL;
            return h.ToString("X12", CultureInfo.InvariantCulture);
        }

        public static uint Crc32(byte[] data, int count)
        {
            uint crc = 0xFFFFFFFFu;
            for (int i = 0; i < count; i++)
            {
                crc ^= data[i];
                for (int k = 0; k < 8; k++) crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
            }
            return ~crc;
        }

        // The PC runtime signs saves with a zero XboxSignatureKey (tools/testing/testing_profile.py).
        public static byte[] Signature(byte[] body, int count)
        {
            using (var hmac = new HMACSHA1(new byte[16])) return hmac.ComputeHash(body, 0, count);
        }

        static uint U32(byte[] b, int o) { return (uint)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24); }
        static void Put32(byte[] b, int o, uint v) { b[o] = (byte)v; b[o + 1] = (byte)(v >> 8); b[o + 2] = (byte)(v >> 16); b[o + 3] = (byte)(v >> 24); }

        public static void ValidName(string name)
        {
            Core.Require(name != null && name.Length > 0 && name.Length <= MaxNameBytes && name.Trim() == name &&
                name.All(c => c >= 0x20 && c <= 0x7E), "Map names must be 1-" + MaxNameBytes + " plain ASCII characters without leading or trailing spaces.");
        }

        static string BodyName(byte[] body)
        {
            int end = Array.IndexOf(body, (byte)0, NameOffset, NameField);
            Core.Require(end > NameOffset, "The map's name field is damaged.");
            return Encoding.ASCII.GetString(body, NameOffset, end - NameOffset);
        }

        // Validates a complete anicemap.mkr against what this build can load. Throws
        // InvalidDataException with a user-facing reason.
        public static MapEntry Parse(byte[] file, string name)
        {
            Core.Require(file != null && file.Length > SignatureSize, "The map file is empty.");
            int body = file.Length - SignatureSize;
            Core.Require(Tiers.Contains(body), "The map file size (" + file.Length + " bytes) is not a MapMaker size tier.");
            Core.Require(U32(file, 0) == Magic && U32(file, 4) == Version, "This is not a supported MapMaker map (unknown header or version).");
            Core.Require(U32(file, 8) == InfoTag && U32(file, 12) >= 0x24C, "The map information chunk is missing.");
            uint used = U32(file, UsedOffset);
            Core.Require(used >= 24 && used <= (uint)body - 4, "The map's stored size does not fit its file.");
            Core.Require(U32(file, (int)used) == Crc32(file, (int)used), "The map's CRC check failed; the file is damaged.");
            long offset = 8; bool ended = false;
            for (int chunks = 0; offset + 8 <= used && chunks < 100000; chunks++)
            {
                uint tag = U32(file, (int)offset), size = U32(file, (int)offset + 4);
                Core.Require((tag & 0xFFFFFF00u) == 0x00C77600u, "The map has an unknown chunk.");
                if (tag == EndTag) { Core.Require(size == offset && offset + 8 == used, "The map's end marker is damaged."); ended = true; break; }
                Core.Require(size <= used, "A map chunk is larger than the map.");
                offset += 8 + size;
            }
            Core.Require(ended, "The map's chunk list is damaged.");
            var map = new MapEntry { BodySize = body, Used = (int)used, Pieces = (int)U32(file, PiecesOffset), Tileset = file[TilesetOffset] | file[TilesetOffset + 1] << 8 };
            Core.Require(map.Tileset < MaxTileset, "The map uses an unknown tileset.");
            Core.Require(map.Pieces <= MaxPieces, "The map has " + map.Pieces + " pieces; this build supports up to " + MaxPieces + ".");
            BodyName(file);
            map.Name = name ?? BodyName(file); ValidName(map.Name);
            var expected = Signature(file, body);
            map.SignatureMatches = expected.SequenceEqual(file.Skip(body));
            return map;
        }

        static string ReadMetaName(string path)
        {
            Core.Require(new FileInfo(path).Length <= 4096, "Oversized save metadata.");
            string text = Encoding.Unicode.GetString(File.ReadAllBytes(path)).TrimStart('﻿');
            foreach (string line in text.Split('\n'))
            {
                string value = line.TrimEnd('\r');
                if (value.StartsWith("Name=", StringComparison.Ordinal)) return value.Substring(5);
            }
            return null;
        }

        static string Title(string saves) { return Core.PathIn(saves, TitleFolder); }

        // Lists the maps in a save folder (read-only). Damaged maps are listed with Problem set.
        public static MapEntry[] List(string saves)
        {
            string title = Title(saves);
            if (!Directory.Exists(title)) return new MapEntry[0];
            var result = new List<MapEntry>();
            foreach (string dir in Directory.GetDirectories(title).OrderBy(d => d, StringComparer.Ordinal))
            {
                string folder = Path.GetFileName(dir), meta = Path.Combine(dir, MetaFile), file = Path.Combine(dir, MapFile);
                if (!SaveDirectory.IsMatch(folder) || !File.Exists(meta) || !File.Exists(file)) continue;
                var entry = new MapEntry { Folder = folder };
                try
                {
                    string full = ReadMetaName(meta);
                    string prefix = full == null ? null : Prefixes.FirstOrDefault(p => full.StartsWith(p, StringComparison.Ordinal));
                    Core.Require(prefix != null, "Not a MapMaker save.");
                    Core.RequireUnlinked(file, "Linked map files are not supported.");
                    Core.Require(new FileInfo(file).Length <= Tiers.Last() + SignatureSize, "The map file is too large.");
                    var parsed = Parse(File.ReadAllBytes(file), full.Substring(prefix.Length));
                    parsed.Folder = folder; parsed.Prefix = prefix; entry = parsed;
                }
                catch (Exception e)
                {
                    if (!(e is IOException || e is InvalidDataException || e is UnauthorizedAccessException)) throw;
                    entry.Problem = e.Message;
                    try { entry.Name = ReadMetaName(meta); } catch (IOException) { }
                }
                result.Add(entry);
            }
            return result.ToArray();
        }

        public static byte[] Pack(MapEntry map, byte[] file, string gameVersion)
        {
            var meta = new Dictionary<string, object> {
                {"format", 1}, {"title_id", "45410066"}, {"name", map.Name}, {"body_size", map.BodySize}, {"tier", map.Tier},
                {"used", map.Used}, {"pieces", map.Pieces}, {"tileset", map.Tileset}, {"container_version", (int)Version},
                {"crc32", U32(file, map.Used).ToString("x8", CultureInfo.InvariantCulture)},
                {"launcher", LauncherVersion.Value}, {"game", gameVersion ?? ""},
                {"exported_utc", DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ", CultureInfo.InvariantCulture)} };
            byte[] header = Encoding.UTF8.GetBytes(Json.Encode(meta));
            using (var output = new MemoryStream())
            {
                output.Write(PackageMagic, 0, PackageMagic.Length);
                output.Write(BitConverter.GetBytes((uint)header.Length), 0, 4); output.Write(header, 0, header.Length);
                output.Write(BitConverter.GetBytes((uint)file.Length), 0, 4); output.Write(file, 0, file.Length);
                byte[] content = output.ToArray();
                using (var sha = SHA256.Create()) { byte[] hash = sha.ComputeHash(content); output.Write(hash, 0, hash.Length); }
                return output.ToArray();
            }
        }

        static int Number(Dictionary<string, object> meta, string key)
        {
            int value; Core.Require(int.TryParse(Json.String(meta, key), NumberStyles.None, CultureInfo.InvariantCulture, out value), "The map file's " + key + " is missing.");
            return value;
        }

        public static MapPackage Unpack(byte[] package)
        {
            const string damaged = "This is not a valid .tsmap file, or it is damaged.";
            Core.Require(package != null && package.Length > 48 && package.Take(8).SequenceEqual(PackageMagic), damaged);
            using (var sha = SHA256.Create())
                Core.Require(sha.ComputeHash(package, 0, package.Length - 32).SequenceEqual(package.Skip(package.Length - 32)), damaged + " (checksum mismatch)");
            uint headerLength = U32(package, 8);
            Core.Require(headerLength > 0 && headerLength <= MaxHeader && 12 + headerLength + 4 <= package.Length - 32, damaged);
            uint fileLength = U32(package, 12 + (int)headerLength);
            Core.Require(fileLength <= Tiers.Last() + SignatureSize && 16 + headerLength + fileLength + 32 == package.Length, damaged);
            Dictionary<string, object> meta;
            try { meta = Json.Object(Encoding.UTF8.GetString(package, 12, (int)headerLength)); }
            catch (Exception e) { if (e is OutOfMemoryException) throw; throw new InvalidDataException(damaged); }
            Core.Require(meta != null && Json.String(meta, "format") == "1", "This .tsmap file needs a newer launcher.");
            Core.Require(Json.String(meta, "title_id") == "45410066", "This map is for a different game.");
            var file = new byte[fileLength]; Buffer.BlockCopy(package, 16 + (int)headerLength, file, 0, (int)fileLength);
            var map = Parse(file, Json.String(meta, "name"));
            Core.Require(Number(meta, "body_size") == map.BodySize && Number(meta, "pieces") == map.Pieces && Number(meta, "used") == map.Used,
                "The map file's description does not match its contents.");
            return new MapPackage { Metadata = meta, File = file, Map = map };
        }

        public static string SafeFileName(string name)
        {
            var text = new StringBuilder();
            foreach (char c in name) text.Append(char.IsLetterOrDigit(c) || c == '-' || c == '_' || c == ' ' ? c : '_');
            string result = text.ToString().Trim();
            return result.Length == 0 ? "map" : result;
        }

        static void CheckClosed(string action)
        {
            Core.Require(!Core.GameRunning(), "Close the game before " + action + " MapMaker maps.");
        }

        // Writes a .tsmap for one map of the active save folder. target null = Backups/Maps.
        // Read-only for saves. Returns the written path.
        public static string Export(string root, string folder, string target, bool installationLockHeld = false)
        {
            Directory.CreateDirectory(Core.PathIn(root, ".launcher"));
            using (LauncherPlatform.AcquireSaveLock(root, installationLockHeld))
            {
                CheckClosed("exporting");
                string saves = SaveLocations.Resolve(root);
                var map = List(saves).FirstOrDefault(m => m.Folder == folder);
                Core.Require(map != null, "That map is no longer in the save folder.");
                Core.Require(map.Valid, "This map cannot be exported: " + map.Problem);
                byte[] file = File.ReadAllBytes(Path.Combine(Title(saves), folder, MapFile));
                byte[] package = Pack(map, file, Core.InstalledVersion(root));
                if (target == null)
                {
                    string exports = Core.PathIn(root, ExportFolder); Directory.CreateDirectory(exports);
                    string stem = SafeFileName(map.Name); target = Path.Combine(exports, stem + Extension);
                    for (int n = 2; File.Exists(target); n++) target = Path.Combine(exports, stem + " (" + n + ")" + Extension);
                }
                target = Path.GetFullPath(target);
                Core.Require(!target.StartsWith(Path.GetFullPath(saves).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, SaveLocations.Comparison),
                    "Choose a location outside the save folder.");
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                string tmp = target + ".new";
                File.WriteAllBytes(tmp, package);
                Core.Require(Unpack(File.ReadAllBytes(tmp)).File.SequenceEqual(file), "The exported file did not verify.");
                if (File.Exists(target)) File.Replace(tmp, target, null); else File.Move(tmp, target);
                return target;
            }
        }

        // Picks a name not used by any listed map (any prefix, ignoring case) whose save
        // directory does not exist either.
        public static string UniqueName(string saves, string name, string prefix)
        {
            ValidName(name);
            string title = Title(saves);
            var names = new HashSet<string>(List(saves).Where(m => m.Name != null).Select(m => m.Name), StringComparer.OrdinalIgnoreCase);
            for (int n = 1; n < 1000; n++)
            {
                string suffix = n == 1 ? "" : " (" + n + ")";
                string candidate = (name.Length + suffix.Length > MaxNameBytes ? name.Substring(0, MaxNameBytes - suffix.Length).TrimEnd() : name) + suffix;
                if (names.Contains(candidate) || Directory.Exists(Path.Combine(title, DirectoryName(prefix + candidate)))) continue;
                return candidate;
            }
            throw new InvalidDataException("No free name for this map; rename some maps first.");
        }

        // Rewrites the body's name field, CRC and signature for the target save folder.
        public static byte[] Rename(byte[] file, string name)
        {
            ValidName(name);
            int body = file.Length - SignatureSize;
            var result = (byte[])file.Clone();
            string old = BodyName(result);
            if (old != name)
            {
                byte[] bytes = Encoding.ASCII.GetBytes(name);
                for (int i = old.Length + 1; i < Math.Min(NameField, bytes.Length + 1); i++)
                    Core.Require(result[NameOffset + i] == 0, "The map's name field cannot hold a longer name.");
                for (int i = 0; i < Math.Max(old.Length, bytes.Length) + 1 && i < NameField; i++) result[NameOffset + i] = i < bytes.Length ? bytes[i] : (byte)0;
            }
            int used = (int)U32(result, UsedOffset);
            Put32(result, used, Crc32(result, used));
            Buffer.BlockCopy(Signature(result, body), 0, result, body, SignatureSize);
            return result;
        }

        static void RemoveStaging(string dir)
        {
            foreach (string file in Directory.GetFiles(dir))
            {
                string leaf = Path.GetFileName(file);
                Core.Require(leaf == MapFile || leaf == MetaFile, "Unexpected file in a map import folder; it was kept.");
            }
            Core.Require(Directory.GetDirectories(dir).Length == 0, "Unexpected folder in a map import folder; it was kept.");
            foreach (string file in Directory.GetFiles(dir)) File.Delete(file);
            Directory.Delete(dir);
        }

        // Adds the package's map to a save folder as a new save directory. Never
        // replaces or edits existing files. No lock/backup: use Import.
        public static MapEntry ImportInto(string saves, MapPackage package, string prefix = "MapMaker: ")
        {
            Core.Require(Prefixes.Contains(prefix), "Unknown map prefix.");
            string title = Title(saves); Directory.CreateDirectory(title);
            foreach (string dir in Directory.GetDirectories(title))
                if (Staging.IsMatch(Path.GetFileName(dir))) RemoveStaging(dir);
            string name = UniqueName(saves, package.Map.Name, prefix);
            byte[] file = Rename(package.File, name);
            Parse(file, name);
            string folder = DirectoryName(prefix + name), final = Path.Combine(title, folder);
            string staging = Path.Combine(title, ".tsmap-import-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(staging);
            try
            {
                using (var output = new FileStream(Path.Combine(staging, MapFile), FileMode.CreateNew, FileAccess.Write)) output.Write(file, 0, file.Length);
                byte[] meta = Encoding.Unicode.GetBytes("﻿Name=" + prefix + name + "\r\n");
                using (var output = new FileStream(Path.Combine(staging, MetaFile), FileMode.CreateNew, FileAccess.Write)) output.Write(meta, 0, meta.Length);
                Core.Require(!Directory.Exists(final), "A save with this name appeared during import; nothing was replaced.");
                Directory.Move(staging, final);
            }
            catch { if (Directory.Exists(staging)) RemoveStaging(staging); throw; }
            var added = List(saves).FirstOrDefault(m => m.Folder == folder);
            Core.Require(added != null && added.Valid && added.Name == name, "The imported map did not verify.");
            return added;
        }

        public static MapPackage ReadPackage(string path)
        {
            Core.Require(new FileInfo(path).Length <= Tiers.Last() + SignatureSize + MaxHeader + 64, "This file is too large to be a .tsmap map.");
            return Unpack(File.ReadAllBytes(path));
        }

        // Validates the file, backs up the save folder, then adds the map under a free name.
        public static MapEntry Import(string root, string packagePath, IProgress<string> progress, CancellationToken token, bool installationLockHeld = false)
        {
            var package = ReadPackage(packagePath);
            Directory.CreateDirectory(Core.PathIn(root, ".launcher"));
            using (LauncherPlatform.AcquireSaveLock(root, installationLockHeld))
            {
                CheckClosed("importing");
                SaveBackups.Recover(root);
                string saves = SaveLocations.Resolve(root);
                SaveBackups.CreateLocked(root, BackupReason, progress, token);
                token.ThrowIfCancellationRequested();
                CheckClosed("importing");
                return ImportInto(saves, package);
            }
        }
    }
}
