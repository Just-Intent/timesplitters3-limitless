using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using TimeSplitters.Launcher;

// MapMaker map export/import on synthetic save folders (never personal saves).
public static class MapTransferChecks
{
    static void Need(bool value,string why){if(!value)throw new Exception(why);}
    static void Reject(Action action,string expected=null)
    {
        string message=null;try{action();}catch(Exception ex){if(!(ex is IOException || ex is InvalidDataException))throw;message=ex.Message;}
        Need(message!=null,"Unsafe map operation was accepted");
        Need(expected==null || message.IndexOf(expected,StringComparison.OrdinalIgnoreCase)>=0,"Unexpected refusal: "+message);
    }
    static void Put(byte[] b,int o,uint v){b[o]=(byte)v;b[o+1]=(byte)(v>>8);b[o+2]=(byte)(v>>16);b[o+3]=(byte)(v>>24);}
    static uint Get(byte[] b,int o){return (uint)(b[o]|b[o+1]<<8|b[o+2]<<16|b[o+3]<<24);}
    // Same layout as a real save: header, 0x68 info chunk (pieces +4, tileset +0xA,
    // name +0x62, used +0x248), a filler chunk, end chunk, CRC at used, zero tail, signature.
    public static byte[] Map(string name,int pieces,int body=0x7FE0,int filler=256)
    {
        var b=new byte[body+20];Put(b,0,0xC77667);Put(b,4,0x12D);Put(b,8,0xC77668);Put(b,12,0x3DC);
        Put(b,16,0x3C4);Put(b,20,(uint)pieces);b[26]=3;Encoding.ASCII.GetBytes(name).CopyTo(b,16+0x62);
        int off=16+0x3DC;Put(b,off,0xC77672);Put(b,off+4,(uint)filler);for(int i=0;i<filler;i++)b[off+8+i]=(byte)(i*7+1);off+=8+filler;
        Put(b,off,0xC7766A);Put(b,off+4,(uint)off);int used=off+8;Put(b,16+0x248,(uint)used);
        Put(b,used,MapTransfer.Crc32(b,used));MapTransfer.Signature(b,body).CopyTo(b,body);return b;
    }
    static string Saves(string root){return Core.PathIn(root,LauncherPlatform.Saves(root));}
    static string Title(string root){return Path.Combine(Saves(root),"UserData","45410066");}
    static void SaveMap(string root,string name,byte[] file)
    {
        string dir=Path.Combine(Title(root),MapTransfer.DirectoryName("MapMaker: "+name));Directory.CreateDirectory(dir);
        File.WriteAllBytes(Path.Combine(dir,"anicemap.mkr"),file);File.WriteAllBytes(Path.Combine(dir,"SaveMeta.xbx"),Encoding.Unicode.GetBytes("﻿Name=MapMaker: "+name+"\r\n"));
    }
    public static string Fixture(string parent,string name)
    {
        string root=Path.Combine(parent,name);string profile=Path.Combine(Title(root),MapTransfer.DirectoryName("Player 1"));Directory.CreateDirectory(profile);
        File.WriteAllBytes(Path.Combine(profile,"SaveMeta.xbx"),Encoding.Unicode.GetBytes("﻿Name=Player 1\r\n"));File.WriteAllBytes(Path.Combine(profile,"tsftrpft"),Enumerable.Repeat((byte)0x5A,8040).ToArray());
        return root;
    }
    static Dictionary<string,string> Tree(string root)
    {
        string saves=Saves(root);if(!Directory.Exists(saves))return new Dictionary<string,string>();
        using(var sha=SHA256.Create())return Directory.GetFiles(saves,"*",SearchOption.AllDirectories).ToDictionary(f=>f.Substring(saves.Length),f=>Convert.ToBase64String(sha.ComputeHash(File.ReadAllBytes(f))));
    }
    static bool Same(Dictionary<string,string> a,Dictionary<string,string> b){return a.Count==b.Count && a.All(p=>b.ContainsKey(p.Key) && b[p.Key]==p.Value);}
    static byte[] Reseal(byte[] package){var copy=(byte[])package.Clone();using(var sha=SHA256.Create())sha.ComputeHash(copy,0,copy.Length-32).CopyTo(copy,copy.Length-32);return copy;}
    public static void Run(string parent,Action<string,Action> test)
    {
        test("map save folders use the game's XAPI name hash and maps are listed per save folder",()=>{
            Need(MapTransfer.DirectoryName("MapMaker: s0")=="7DC83E79F248" && MapTransfer.DirectoryName("MapMaker: t2")=="7DC83E7AF24A","Map folder hash differs from the game");
            Need(MapTransfer.DirectoryName("MapMaker: bigmap2")=="EACA7005BA26" && MapTransfer.DirectoryName("Player 1")=="16D15BD8D404","Save folder hash differs from the game");
            string root=Fixture(parent,"maps-list");SaveMap(root,"Arena",Map("Arena",40));SaveMap(root,"Broken",Map("Broken",10));
            string broken=Path.Combine(Title(root),MapTransfer.DirectoryName("MapMaker: Broken"),"anicemap.mkr");var bytes=File.ReadAllBytes(broken);bytes[600]^=1;File.WriteAllBytes(broken,bytes);
            var maps=MapTransfer.List(Saves(root));
            Need(maps.Length==2 && maps.Count(m=>m.Valid)==1,"Profile listed as a map or damaged map hidden");
            var arena=maps.Single(m=>m.Valid);Need(arena.Name=="Arena" && arena.Pieces==40 && arena.Tileset==3 && arena.Tier=="32 KB" && arena.SignatureMatches,"Map details wrong");
            Need(maps.Single(m=>!m.Valid).Problem.Contains("CRC"),"Damaged map not explained");
        });
        test("map export and import round-trip between save folders without changing other files",()=>{
            string source=Fixture(parent,"maps-source"),target=Fixture(parent,"maps-target");
            byte[] original=Map("Arena",120);SaveMap(source,"Arena",original);
            var sourceBefore=Tree(source);string folder=MapTransfer.List(Saves(source)).Single().Folder;
            string exported=MapTransfer.Export(source,folder,null);
            Need(exported.EndsWith(Path.Combine("Backups","Maps","Arena.tsmap")),"Default export location wrong: "+exported);
            Need(Same(sourceBefore,Tree(source)),"Export changed the save folder");
            string second=MapTransfer.Export(source,folder,null);Need(second.EndsWith("Arena (2).tsmap") && File.Exists(exported),"Export overwrote an earlier export");
            var package=MapTransfer.ReadPackage(exported);Need(package.Map.Pieces==120 && Json.String(package.Metadata,"tier")=="32 KB","Package metadata wrong");
            var targetBefore=Tree(target);
            var added=MapTransfer.Import(target,exported,null,CancellationToken.None);
            Need(added.Name=="Arena" && added.Folder==folder,"Imported map name or folder wrong");
            Need(File.ReadAllBytes(Path.Combine(Title(target),folder,"anicemap.mkr")).SequenceEqual(original),"Imported map bytes differ");
            var targetAfter=Tree(target);Need(targetBefore.All(p=>targetAfter[p.Key]==p.Value) && targetAfter.Count==targetBefore.Count+2,"Import changed or removed other save files");
            var backups=SaveBackups.List(target);Need(backups.Length==1 && backups[0].Reason==MapTransfer.BackupReason,"No backup before import");
            string chosen=MapTransfer.Export(source,folder,Path.Combine(parent,"chosen","arena.tsmap"));
            Need(MapTransfer.ReadPackage(chosen).File.SequenceEqual(original) && Same(sourceBefore,Tree(source)),"Export to a chosen file failed or changed saves");
            Reject(()=>MapTransfer.Export(source,folder,Path.Combine(Saves(source),"inside.tsmap")),"outside");
        });
        test("map import renames on collisions and re-signs the renamed copy",()=>{
            string root=Fixture(parent,"maps-collide");byte[] original=Map("Arena",50);SaveMap(root,"Arena",original);
            string file=MapTransfer.Export(root,MapTransfer.List(Saves(root)).Single().Folder,null);
            var first=MapTransfer.Import(root,file,null,CancellationToken.None);var again=MapTransfer.Import(root,file,null,CancellationToken.None);
            Need(first.Name=="Arena (2)" && again.Name=="Arena (3)","Collision names wrong: "+first.Name+", "+again.Name);
            Need(File.ReadAllBytes(Path.Combine(Title(root),MapTransfer.DirectoryName("MapMaker: Arena"),"anicemap.mkr")).SequenceEqual(original),"Collision changed the existing map");
            byte[] copy=File.ReadAllBytes(Path.Combine(Title(root),first.Folder,"anicemap.mkr"));
            Need(Encoding.ASCII.GetString(copy,16+0x62,9)=="Arena (2)" && copy[16+0x62+9]==0,"Body name not updated");
            Need(first.SignatureMatches && Get(copy,(int)Get(copy,16+0x248))==MapTransfer.Crc32(copy,(int)Get(copy,16+0x248)),"Renamed copy CRC/signature not recomputed");
            Need(Encoding.Unicode.GetString(File.ReadAllBytes(Path.Combine(Title(root),first.Folder,"SaveMeta.xbx")))=="﻿Name=MapMaker: Arena (2)\r\n","Save metadata wrong");
            string longName=new string('L',31);SaveMap(root,longName,Map(longName,5));
            string longFile=MapTransfer.Export(root,MapTransfer.DirectoryName("MapMaker: "+longName),null);
            var longCopy=MapTransfer.Import(root,longFile,null,CancellationToken.None);Need(longCopy.Name==new string('L',27)+" (2)","Long name not shortened to fit");
            Need(MapTransfer.List(Saves(root)).Length==5 && MapTransfer.List(Saves(root)).All(m=>m.Valid),"Imported maps do not all verify");
        });
        test("map import rejects damaged files and maps beyond the game's caps without changing saves",()=>{
            string root=Fixture(parent,"maps-reject");SaveMap(root,"Arena",Map("Arena",30));
            string file=MapTransfer.Export(root,MapTransfer.List(Saves(root)).Single().Folder,null);byte[] package=File.ReadAllBytes(file);
            var before=Tree(root);int backups=SaveBackups.List(root).Length;
            string bad=Path.Combine(parent,"bad.tsmap");
            Action<byte[],string> refused=(bytes,why)=>{File.WriteAllBytes(bad,bytes);Reject(()=>MapTransfer.Import(root,bad,null,CancellationToken.None),why);};
            var flipped=(byte[])package.Clone();flipped[flipped.Length-500]^=0x10;refused(flipped,"damaged");
            int mapStart=package.Length-32-(0x7FE0+20);
            var crc=(byte[])package.Clone();crc[mapStart+700]^=0x10;refused(Reseal(crc),"CRC");
            refused(package.Take(package.Length-1).ToArray(),"damaged");refused(Encoding.ASCII.GetBytes("not a map"),"damaged");
            var header=(byte[])package.Clone();Put(header,mapStart+4,0x12C);refused(Reseal(header),"version");
            byte[] many=Map("Crowded",201);
            Reject(()=>MapTransfer.Parse(many,"Crowded"),"up to 200");Need(MapTransfer.Parse(Map("Full",200),"Full").Pieces==200,"A 200-piece map was refused");Reject(()=>MapTransfer.Parse(many.Take(0x6000).ToArray(),"Short"),"map size");
            Reject(()=>MapTransfer.Parse(Map("Arena",5),"badéname"),"ASCII");
            Need(Same(before,Tree(root)) && SaveBackups.List(root).Length==backups,"Rejected import changed saves or made a backup");
        });
        test("maps larger than the game's 32 KB map size are refused with a clear reason",()=>{
            string root=Fixture(parent,"maps-large");SaveMap(root,"Arena",Map("Arena",30));byte[] large=Map("Huge",150,0xFFE0,0x9000);SaveMap(root,"Huge",large);
            Reject(()=>MapTransfer.Parse(large,"Huge"),"larger than the 32 KB");
            var listed=MapTransfer.List(Saves(root)).Single(m=>!m.Valid);Need(listed.Problem.Contains("32 KB") && listed.Name=="MapMaker: Huge","Large map not explained in the list");
            Reject(()=>MapTransfer.Export(root,listed.Folder,null),"cannot be exported");
            var entry=new MapEntry{Name="Huge",BodySize=0xFFE0,Used=(int)Get(large,16+0x248),Pieces=150,Tileset=3};
            string file=Path.Combine(parent,"large.tsmap");File.WriteAllBytes(file,MapTransfer.Pack(entry,large,"test"));
            var before=Tree(root);int backups=SaveBackups.List(root).Length;
            Reject(()=>MapTransfer.Import(root,file,null,CancellationToken.None),"larger than the 32 KB");
            Need(Same(before,Tree(root)) && SaveBackups.List(root).Length==backups,"Refused large import changed saves or made a backup");
        });
        test("map import removes only its own interrupted staging folders",()=>{
            string root=Fixture(parent,"maps-staging");SaveMap(root,"Arena",Map("Arena",30));
            string file=MapTransfer.Export(root,MapTransfer.List(Saves(root)).Single().Folder,null);
            string stale=Path.Combine(Title(root),".tsmap-import-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stale);File.WriteAllBytes(Path.Combine(stale,"anicemap.mkr"),new byte[3]);
            MapTransfer.Import(root,file,null,CancellationToken.None);Need(!Directory.Exists(stale),"Interrupted staging folder kept");
            Directory.CreateDirectory(stale);File.WriteAllText(Path.Combine(stale,"user-notes.txt"),"keep");
            Reject(()=>MapTransfer.Import(root,file,null,CancellationToken.None),"kept");Need(File.ReadAllText(Path.Combine(stale,"user-notes.txt"))=="keep","Unknown staging content removed");
        });
    }
}
