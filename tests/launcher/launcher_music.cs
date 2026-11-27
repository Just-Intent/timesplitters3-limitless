using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using TimeSplitters.Launcher;

// Imported music (MusicLibrary) on synthetic install folders; never personal music.
public static class MusicLibraryChecks
{
    static void Need(bool value,string why){if(!value)throw new Exception(why);}
    static void Reject(Action action,string expected=null)
    {
        string message=null;try{action();}catch(Exception ex){if(!(ex is IOException || ex is InvalidDataException))throw;message=ex.Message;}
        Need(message!=null,"Unsafe music operation was accepted");
        Need(expected==null || message.IndexOf(expected,StringComparison.OrdinalIgnoreCase)>=0,"Unexpected refusal: "+message);
    }
    public static byte[] Wav(int frames=64)
    {
        var data=new byte[frames*4];var b=new MemoryStream();var w=new BinaryWriter(b);
        w.Write(Encoding.ASCII.GetBytes("RIFF"));w.Write(36+data.Length);w.Write(Encoding.ASCII.GetBytes("WAVEfmt "));w.Write(16);
        w.Write((short)1);w.Write((short)2);w.Write(44100);w.Write(44100*4);w.Write((short)4);w.Write((short)16);
        w.Write(Encoding.ASCII.GetBytes("data"));w.Write(data.Length);w.Write(data);return b.ToArray();
    }
    public static byte[] Mp3(bool tag) { var b=new byte[600];if(tag){b[0]=(byte)'I';b[1]=(byte)'D';b[2]=(byte)'3';b[3]=4;}else{b[0]=0xFF;b[1]=0xFB;b[2]=0x90;}return b; }
    static string Music(string root){return Core.PathIn(root,LauncherPlatform.Music(root));}
    static string Source(string parent,string name,byte[] bytes){string dir=Path.Combine(parent,"chosen");Directory.CreateDirectory(dir);string path=Path.Combine(dir,name);File.WriteAllBytes(path,bytes);return path;}
    public static string Fixture(string parent,string name){string root=Path.Combine(parent,name);Directory.CreateDirectory(root);return root;}
    public static void Run(string parent,Action<string,Action> test)
    {
        test("music names follow the game's rules and imports never replace a track",()=>{
            Need(MusicLibrary.GameName("Song.wav") && MusicLibrary.GameName("b.MP3") && !MusicLibrary.GameName(".x.wav") && !MusicLibrary.GameName("a.ogg") &&
                !MusicLibrary.GameName("a:b.wav") && !MusicLibrary.GameName(new string('a',60)+".wav") && MusicLibrary.GameName(new string('a',59)+".wav"),"Name rules differ from pc_music_name_ok");
            string accented=Path.Combine("x","Café del Mar — 01.MP3");
            Need(MusicLibrary.SafeName(accented)=="Caf_ del Mar _ 01.mp3","Unsafe characters kept: "+MusicLibrary.SafeName(accented));
            Need(MusicLibrary.SafeName("con.wav")=="_con.wav" && MusicLibrary.SafeName("...wav")=="Track.wav","Reserved or empty names kept");
            string longName=MusicLibrary.SafeName(new string('x',200)+".wav");Need(MusicLibrary.GameName(longName) && Encoding.UTF8.GetByteCount(longName)<=MusicLibrary.MaxNameBytes-4,"Long name not shortened with room for a suffix");
            string root=Fixture(parent,"music-import");
            Need(MusicLibrary.List(root).Length==0,"Missing folder listed tracks");
            string a=Source(parent,"Theme.wav",Wav()),b=Source(parent,"Boss Fight.mp3",Mp3(true)),c=Source(parent,"Plain.mp3",Mp3(false));
            var added=MusicLibrary.Import(root,new[]{a,b,c},null,CancellationToken.None);
            Need(added.Select(t=>t.Name).SequenceEqual(new[]{"Theme.wav","Boss Fight.mp3","Plain.mp3"}),"Imported names differ");
            Need(File.ReadAllBytes(Path.Combine(Music(root),"Theme.wav")).SequenceEqual(Wav()) && File.Exists(a),"Copy differs or original moved");
            File.WriteAllBytes(Path.Combine(Music(root),"theme (2).WAV"),Wav(8));                       // the player's own file
            added=MusicLibrary.Import(root,new[]{a,a},null,CancellationToken.None);
            Need(added.Select(t=>t.Name).SequenceEqual(new[]{"Theme (3).wav","Theme (4).wav"}),"Name collision replaced or reused a track: "+string.Join(",",added.Select(t=>t.Name)));
            Need(File.ReadAllBytes(Path.Combine(Music(root),"theme (2).WAV")).Length==Wav(8).Length,"Existing track was replaced");
            File.WriteAllText(Path.Combine(Music(root),"notes.txt"),"x");File.WriteAllBytes(Path.Combine(Music(root),".hidden.wav"),Wav());
            var listed=MusicLibrary.List(root).Select(t=>t.Name).ToArray();
            Need(listed.Length==6 && listed[0]=="Boss Fight.mp3" && !listed.Contains("notes.txt") && !listed.Contains(".hidden.wav"),"List shows files the game skips: "+string.Join(",",listed));
            Need(!Directory.GetFiles(Music(root)).Any(f=>Path.GetFileName(f).StartsWith(".music-import-")),"Staging file left behind");
        });
        test("music import refuses files the game cannot play and leaves the folder unchanged",()=>{
            string root=Fixture(parent,"music-refuse");
            Reject(()=>MusicLibrary.Import(root,new[]{Source(parent,"song.ogg",new byte[10])},null,CancellationToken.None),"not a WAV or MP3");
            Reject(()=>MusicLibrary.Import(root,new[]{Source(parent,"fake.wav",Mp3(true))},null,CancellationToken.None),"not a WAV");
            Reject(()=>MusicLibrary.Import(root,new[]{Source(parent,"fake.mp3",Encoding.ASCII.GetBytes("hello world, not audio"))},null,CancellationToken.None),"not an MP3");
            Reject(()=>MusicLibrary.Import(root,new[]{Source(parent,"empty.mp3",new byte[0])},null,CancellationToken.None),"empty");
            Reject(()=>MusicLibrary.Import(root,new[]{Path.Combine(parent,"chosen","absent.wav")},null,CancellationToken.None),"not found");
            Reject(()=>MusicLibrary.Import(root,new string[0],null,CancellationToken.None),"at least one");
            // one bad file in a batch stops the whole batch before anything is copied
            Reject(()=>MusicLibrary.Import(root,new[]{Source(parent,"good.wav",Wav()),Source(parent,"bad.mp3",new byte[4])},null,CancellationToken.None),"not an MP3");
            Need(!Directory.Exists(Music(root)) || Directory.GetFiles(Music(root)).Length==0,"Refused import wrote files");
            Directory.CreateDirectory(Music(root));
            for(int i=0;i<MusicLibrary.MaxTracks-1;i++)File.WriteAllBytes(Path.Combine(Music(root),"t"+i.ToString("00")+".wav"),Wav(1));
            string one=Source(parent,"one.wav",Wav());
            Reject(()=>MusicLibrary.Import(root,new[]{one,one},null,CancellationToken.None),"up to "+MusicLibrary.MaxTracks);
            Need(MusicLibrary.Import(root,new[]{one},null,CancellationToken.None).Length==1 && MusicLibrary.List(root).Length==MusicLibrary.MaxTracks,"Last free slot refused");
            Reject(()=>MusicLibrary.Import(root,new[]{Path.Combine(Music(root),"t00.wav")},null,CancellationToken.None),"up to");
            string cancelRoot=Fixture(parent,"music-cancel");var stop=new CancellationTokenSource();stop.Cancel();
            bool cancelled=false;try{MusicLibrary.Import(cancelRoot,new[]{one},null,stop.Token);}catch(OperationCanceledException){cancelled=true;}
            Need(cancelled && Directory.GetFiles(Music(cancelRoot)).Length==0,"Cancelled import left a file");
            string inside=Fixture(parent,"music-inside");Directory.CreateDirectory(Music(inside));string own=Path.Combine(Music(inside),"own.wav");File.WriteAllBytes(own,Wav());
            Reject(()=>MusicLibrary.Import(inside,new[]{own},null,CancellationToken.None),"already in the music folder");
        });
        test("music removal goes through the host's recycle adapter for listed tracks only",()=>{
            string root=Fixture(parent,"music-remove");Directory.CreateDirectory(Music(root));
            File.WriteAllBytes(Path.Combine(Music(root),"keep.wav"),Wav());File.WriteAllBytes(Path.Combine(Music(root),"gone.mp3"),Mp3(true));File.WriteAllText(Path.Combine(Music(root),"notes.txt"),"x");
            string recycled=null;Action<string> recycle=p=>{recycled=p;File.Delete(p);};
            Reject(()=>MusicLibrary.Remove(root,"gone.mp3",null),"not available");
            Reject(()=>MusicLibrary.Remove(root,"notes.txt",recycle),"not an imported track");
            Reject(()=>MusicLibrary.Remove(root,"../keep.wav",recycle),"not an imported track");
            Reject(()=>MusicLibrary.Remove(root,"absent.wav",recycle),"no longer");
            Need(recycled==null,"A refused removal reached the adapter");
            MusicLibrary.Remove(root,"gone.mp3",recycle);
            Need(recycled==Path.Combine(Music(root),"gone.mp3") && File.Exists(Path.Combine(Music(root),"keep.wav")) && MusicLibrary.List(root).Length==1,"Removal touched the wrong file");
            Reject(()=>MusicLibrary.Remove(root,"keep.wav",p=>{}),"could not be removed");
        });
    }
}
