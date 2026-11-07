using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
#else
using System.Windows;
using System.Windows.Controls;
#endif

namespace TimeSplitters.Launcher
{
    public sealed class SavesPage
    {
        public readonly Grid View=new Grid();
        public readonly Button Backup, Restore, Refresh, OpenSaves, OpenBackups, Cancel;
        public readonly ComboBox Choices=new ComboBox {Name="SaveBackupChoices"};
        public event Action<bool> BusyChanged;
        public event Action<string> OpenFolder;
        public event Action<string> OpenSaveLocation;
        public Func<Task<string>> PickSaveFolder;
        // Host file pickers for map transfer: export receives a suggested file name; both return null on cancel.
        public Func<string,Task<string>> PickMapExportFile;
        public Func<Task<string>> PickMapImportFile;
        public readonly ComboBox MapChoices=new ComboBox {Name="MapChoices"};
        public readonly Button ExportMap, ImportMap, OpenMapExports;
        readonly TextBlock mapDetails;
        // Imported music (MusicLibrary): the host's multi-select WAV/MP3 picker (null on
        // cancel) and its removal to the Recycle Bin or Trash.
        public Func<Task<string[]>> PickMusicFiles;
        public Action<string> RecycleMusic;
        public readonly ComboBox MusicChoices=new ComboBox {Name="MusicChoices"};
        public readonly Button ImportMusic, RemoveMusic, OpenMusic;
        readonly TextBlock musicDetails;
        readonly TextBlock location;
        readonly string root;readonly bool preview,installationLockHeld;
        readonly StackPanel body=new StackPanel();readonly TextBlock status,summary,details;
        readonly Border confirmation;CancellationTokenSource stop;
        public SavesPage(string root,bool preview,bool installationLockHeld=false)
        {
            this.root=root;this.preview=preview;this.installationLockHeld=installationLockHeld;View.Margin=new Thickness(30,24,30,18);View.Background=PageTheme.Brush("#0B1420");Ui.Theme(View);
            var dock=new DockPanel();View.Children.Add(dock);
            var footer=new DockPanel {Margin=new Thickness(0,14,0,0)};DockPanel.SetDock(footer,Dock.Bottom);dock.Children.Add(footer);
            Cancel=Button("Cancel operation","CancelSaveOperation");DockPanel.SetDock(Cancel,Dock.Right);footer.Children.Add(Cancel);Ui.Show(Cancel,false);
            status=PageTheme.Text("Automatic backups are created before game updates and restores.",12);status.Name="SaveManagerStatus";footer.Children.Add(status);
            var scroll=new ScrollViewer {VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled,Content=body};dock.Children.Add(scroll);
            body.Children.Add(PageTheme.Header("SAVE MANAGEMENT","Saves & backups","Protect your progress before updates or testing a different game version."));
            var overview=PageTheme.Section(body,"Your saves","Local backups, labeled by date and game version.");
            location=PageTheme.Text("",12);location.Name="ActiveSaveLocation";overview.Children.Add(location);
            var locations=new WrapPanel {Margin=new Thickness(0,12,0,12)};overview.Children.Add(locations);
            var choose=Button("Choose save folder...","ChooseSaveFolder");var portable=Button("Use game folder","DefaultSaveFolder");locations.Children.Add(choose);locations.Children.Add(portable);
            choose.Click+=async(s,e)=>{try {if(preview){Report("Preview only.");return;}if(PickSaveFolder!=null){string selected=await PickSaveFolder();if(selected!=null)ConfirmLocation(selected);}}catch(Exception ex){Report(ex.Message);}};
            portable.Click+=(s,e)=>ConfirmLocation(SaveLocations.Default(root));
            summary=PageTheme.Text("",14);overview.Children.Add(summary);
            var actions=new WrapPanel {Margin=new Thickness(0,12,0,0)};overview.Children.Add(actions);
            Backup=Button("Back up now","CreateSaveBackup");PageTheme.Primary(Backup);actions.Children.Add(Backup);
            OpenSaves=Button("Open saves folder","OpenSaveFolder");actions.Children.Add(OpenSaves);
            OpenBackups=Button("Open backups folder","OpenSaveBackups");actions.Children.Add(OpenBackups);
            var restore=PageTheme.Section(body,"Restore a backup","Close the game first. Current saves are backed up before replacement. Compatibility varies by game version.");
            restore.Children.Add(Choices);details=PageTheme.Text("Select a restore point.",12,"#B4C7D8");details.Margin=new Thickness(0,10,0,12);restore.Children.Add(details);
            var restoreActions=new WrapPanel();restore.Children.Add(restoreActions);
            Restore=Button("Restore selected backup","RestoreSaveBackup");restoreActions.Children.Add(Restore);
            Refresh=Button("Refresh list","RefreshSaveBackups");restoreActions.Children.Add(Refresh);
            var maps=PageTheme.Section(body,"MapMaker maps","Export a map to a .tsmap file to share it, or move it to another computer or save folder. Importing adds a copy and never replaces a map.");
            maps.Children.Add(MapChoices);mapDetails=PageTheme.Text("No maps found in this save folder.",12,"#B4C7D8");mapDetails.Name="MapDetails";mapDetails.Margin=new Thickness(0,10,0,12);maps.Children.Add(mapDetails);
            var mapActions=new WrapPanel();maps.Children.Add(mapActions);
            ExportMap=Button("Export map...","ExportMap");mapActions.Children.Add(ExportMap);
            ImportMap=Button("Import map...","ImportMap");mapActions.Children.Add(ImportMap);
            OpenMapExports=Button("Open map exports","OpenMapExports");mapActions.Children.Add(OpenMapExports);
            MapChoices.SelectionChanged+=(s,e)=>SelectMap();OpenMapExports.Click+=(s,e)=>Open(MapTransfer.ExportFolder);
            ExportMap.Click+=async(s,e)=>{
                var map=MapChoices.SelectedItem as MapEntry;if(map==null || !map.Valid || stop!=null)return;
                if(preview){Report("Preview only. No maps were exported.");return;}
                string target=null;
                if(PickMapExportFile!=null){try {target=await PickMapExportFile(MapTransfer.SafeFileName(map.Name)+MapTransfer.Extension);}catch(Exception ex){Report(ex.Message);return;}if(target==null)return;}
                await Run(async token=>{string written=await Task.Run(()=>MapTransfer.Export(root,map.Folder,target,installationLockHeld),token);return "Exported "+map.Name+" to "+written+". Your saves are unchanged.";});
            };
            ImportMap.Click+=async(s,e)=>{
                if(stop!=null)return;if(preview){Report("Preview only. No maps were imported.");return;}
                if(PickMapImportFile==null){Report("Choosing a map file is not available here.");return;}
                string source;try {source=await PickMapImportFile();}catch(Exception ex){Report(ex.Message);return;}if(source==null)return;
                await Run(async token=>{
                    var progress=Reporter();var added=await Task.Run(()=>MapTransfer.Import(root,source,progress,token,installationLockHeld),token);
                    return "Imported map as "+added.Name+". Your saves were backed up first; no existing map was changed.";
                });
            };
            var music=PageTheme.Section(body,"Imported music","WAV and MP3 tracks for MapMaker's Map Settings and Arcade's music choice. Importing copies the files; your originals stay where they are.");
            music.Children.Add(MusicChoices);musicDetails=PageTheme.Text("No imported music yet.",12,"#B4C7D8");musicDetails.Name="MusicDetails";musicDetails.Margin=new Thickness(0,10,0,12);music.Children.Add(musicDetails);
            var musicActions=new WrapPanel();music.Children.Add(musicActions);
            ImportMusic=Button("Import music...","ImportMusic");musicActions.Children.Add(ImportMusic);
            RemoveMusic=Button("Remove track...","RemoveMusic");musicActions.Children.Add(RemoveMusic);
            OpenMusic=Button("Open music folder","OpenMusicFolder");musicActions.Children.Add(OpenMusic);
            MusicChoices.SelectionChanged+=(s,e)=>SelectMusic();RemoveMusic.Click+=(s,e)=>ConfirmRemoveMusic();
            OpenMusic.Click+=(s,e)=>{try {if(!preview)Directory.CreateDirectory(MusicLibrary.Folder(root));Open(LauncherPlatform.Music(root));}catch(Exception ex){Report(ex.Message);}};
            ImportMusic.Click+=async(s,e)=>{
                if(stop!=null)return;if(preview){Report("Preview only. No music was imported.");return;}
                if(PickMusicFiles==null){Report("Choosing music files is not available here.");return;}
                string[] sources;try {sources=await PickMusicFiles();}catch(Exception ex){Report(ex.Message);return;}if(sources==null || sources.Length==0)return;
                await Run(async token=>{
                    var progress=Reporter();var added=await Task.Run(()=>MusicLibrary.Import(root,sources,progress,token,installationLockHeld),token);
                    return added.Length==1?"Imported "+added[0].Name+". Choose it in Map Settings or Arcade's music list.":"Imported "+added.Length+" tracks. Choose them in Map Settings or Arcade's music list.";
                });
            };
            PageTheme.Details(body,"About backups & storage",
                PageTheme.Text("Backups remain on this computer until you remove them. Empty virtual-disk regions are omitted, so logical size may exceed storage used.",12),
                PageTheme.Text("Automatic backups run before game updates and restores. Game files, settings and your ISO are kept.",12)).Name="SaveStorageDetails";
            confirmation=new Border {Background=PageTheme.Brush("#122233"),BorderBrush=PageTheme.Brush("#86CDFF"),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(7),Padding=new Thickness(28)};
            View.Children.Add(confirmation);Ui.Show(confirmation,false);
            Choices.SelectionChanged+=(s,e)=>Select();Refresh.Click+=(s,e)=>Reload();
            Backup.Click+=async(s,e)=>await Run(async token=> {
                var progress=Reporter();var item=await Task.Run(()=>SaveBackups.Create(root,"Manual",progress,token,installationLockHeld),token);
                return item==null?"No saves found to back up yet.":"Backup created. Your saves are unchanged.";
            });
            Restore.Click+=(s,e)=>ConfirmRestore();Cancel.Click+=(s,e)=>{if(stop!=null){stop.Cancel();Cancel.IsEnabled=false;}};
            OpenSaves.Click+=(s,e)=>{try {if(!preview && OpenSaveLocation!=null)OpenSaveLocation(SaveLocations.Resolve(root));}catch(Exception ex){Report(ex.Message);}};OpenBackups.Click+=(s,e)=>Open(SaveBackups.Folder);Reload();
        }
        static Button Button(string text,string name) { return new Button {Content=text,Name=name,Margin=new Thickness(0,0,10,0)}; }
        void Open(string path) { if(preview){Report("Preview only. No folders were opened.");return;}if(OpenFolder!=null)OpenFolder(path); }
        public void Report(string text) { status.Text=text; }
        IProgress<string> Reporter() {var current=stop;return new Progress<string>(text=>{if(current!=null && stop==current)Report(text);});}
        public void Reload()
        {
            if(stop!=null)return;
            try {
                try {location.Text=preview?"Save folder: <game folder>/Saves":"Save folder: "+SaveLocations.Resolve(root);}
                catch(Exception ex){location.Text=ex.Message;Report("Choose an available save folder to continue. Existing saves were kept.");}
                string id=(Choices.SelectedItem as SaveBackup)==null?null:((SaveBackup)Choices.SelectedItem).Id;
                var items=preview?new SaveBackup[0]:SaveBackups.List(root,Report);Choices.Items.Clear();foreach(var item in items)Choices.Items.Add(item);
                Choices.SelectedItem=items.FirstOrDefault(i=>i.Id==id)??items.FirstOrDefault();
                ReloadMaps();ReloadMusic();
                summary.Text=items.Length==0?"No backups yet. Select Back up now to protect your progress.":items.Length+" restore point(s) · "+(items.Sum(i=>i.StoredBytes)/1048576.0).ToString("0.0")+" MiB stored";
                Select();
            }catch(Exception ex){Report("Backup list: "+ex.Message);Restore.IsEnabled=false;}
        }
        void Select()
        {
            var item=Choices.SelectedItem as SaveBackup;Restore.IsEnabled=item!=null;
            details.Text=item==null?"No restore point selected.":item.Files+" file(s) · "+(item.StoredBytes/1048576.0).ToString("0.0")+" MiB archive · "+(item.LogicalBytes/1073741824.0).ToString("0.00")+" GiB logical saves";
        }
        void ReloadMaps()
        {
            string folder=(MapChoices.SelectedItem as MapEntry)==null?null:((MapEntry)MapChoices.SelectedItem).Folder;
            MapEntry[] maps;mapDetails.Text="";
            try {maps=preview?new MapEntry[0]:MapTransfer.List(SaveLocations.Resolve(root));}
            catch(Exception ex){if(!(ex is IOException || ex is InvalidDataException || ex is UnauthorizedAccessException))throw;maps=new MapEntry[0];mapDetails.Text="Maps: "+ex.Message;}
            MapChoices.Items.Clear();foreach(var map in maps)MapChoices.Items.Add(map);
            MapChoices.SelectedItem=maps.FirstOrDefault(m=>m.Folder==folder)??maps.FirstOrDefault();SelectMap();
        }
        void SelectMap()
        {
            var map=MapChoices.SelectedItem as MapEntry;ExportMap.IsEnabled=map!=null && map.Valid;
            if(map==null){if(!mapDetails.Text.StartsWith("Maps: ",StringComparison.Ordinal))mapDetails.Text="No maps found in this save folder.";return;}
            mapDetails.Text=!map.Valid?"This map cannot be exported: "+map.Problem:
                map.Pieces+" pieces, "+map.Tier+" file"+(map.BodySize>MapTransfer.Tiers[0]?" (larger than the original game can load)":"")+", save folder "+map.Folder;
        }
        void ReloadMusic()
        {
            string name=(MusicChoices.SelectedItem as MusicTrack)==null?null:((MusicTrack)MusicChoices.SelectedItem).Name;
            MusicTrack[] tracks;musicDetails.Text="";
            try {tracks=preview?new MusicTrack[0]:MusicLibrary.List(root);}
            catch(Exception ex){if(!(ex is IOException || ex is InvalidDataException || ex is UnauthorizedAccessException))throw;tracks=new MusicTrack[0];musicDetails.Text="Music: "+ex.Message;}
            MusicChoices.Items.Clear();foreach(var track in tracks)MusicChoices.Items.Add(track);
            MusicChoices.SelectedItem=tracks.FirstOrDefault(t=>t.Name==name)??tracks.FirstOrDefault();SelectMusic();
        }
        void SelectMusic()
        {
            var track=MusicChoices.SelectedItem as MusicTrack;RemoveMusic.IsEnabled=track!=null;
            if(track==null){if(!musicDetails.Text.StartsWith("Music: ",StringComparison.Ordinal))musicDetails.Text="No imported music yet. Import WAV or MP3 files to use them in MapMaker and Arcade.";return;}
            musicDetails.Text=MusicChoices.Items.Count+" of "+MusicLibrary.MaxTracks+" tracks · "+(track.Bytes/1048576.0).ToString("0.0")+" MiB · "+(track.Name.EndsWith(".mp3",StringComparison.OrdinalIgnoreCase)?"MP3":"WAV");
        }
        void ConfirmRemoveMusic()
        {
            var track=MusicChoices.SelectedItem as MusicTrack;if(track==null || stop!=null)return;
            if(preview){Report("Preview only. No music was removed.");return;}
            var prompt=new StackPanel {VerticalAlignment=VerticalAlignment.Center,MaxWidth=560};confirmation.Child=prompt;
            prompt.Children.Add(PageTheme.Text("Remove this track?",24));
            var text=PageTheme.Text(track.Name+"\n\nIt moves to the Recycle Bin (Trash on Mac). Maps and the Arcade choice that use it play the level's own music instead.",14);text.Margin=new Thickness(0,16,0,20);prompt.Children.Add(text);
            var buttons=new WrapPanel();prompt.Children.Add(buttons);Button no=Button("Keep track","CancelMusicRemove"),yes=Button("Remove track","ConfirmMusicRemove");PageTheme.Primary(yes);buttons.Children.Add(no);buttons.Children.Add(yes);
            body.IsEnabled=false;Ui.Show(confirmation,true);
            no.Click+=(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;};
            yes.Click+=async(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;await Run(async token=>{
                await Task.Run(()=>MusicLibrary.Remove(root,track.Name,RecycleMusic,installationLockHeld),token);return "Removed "+track.Name+".";
            });};
        }
        void ConfirmLocation(string selected)
        {
            var prompt=new StackPanel {VerticalAlignment=VerticalAlignment.Center,MaxWidth=620};confirmation.Child=prompt;
            prompt.Children.Add(PageTheme.Text("Choose save folder",24));
            prompt.Children.Add(PageTheme.Text(selected+"\n\nUse saves already in this folder, or copy your current saves into an empty folder. Originals are always kept. This applies to the next game launch.",14));
            var buttons=new WrapPanel {Margin=new Thickness(0,20,0,0)};prompt.Children.Add(buttons);
            foreach(bool copy in new[]{false,true}) {
                bool copying=copy;var yes=Button(copy?"Copy current saves":"Use this folder",copy?"CopySaveLocation":"UseSaveLocation");buttons.Children.Add(yes);
                yes.Click+=async(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;await Run(async token=>{await Task.Run(()=>SaveLocations.Choose(root,selected,copying,token,installationLockHeld),token);return "Save folder changed. Original saves were kept.";});};
            }
            var no=Button("Cancel","CancelSaveLocation");buttons.Children.Add(no);no.Click+=(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;};
            body.IsEnabled=false;Ui.Show(confirmation,true);
        }
        void ConfirmRestore()
        {
            var item=Choices.SelectedItem as SaveBackup;if(item==null || stop!=null)return;
            var prompt=new StackPanel {VerticalAlignment=VerticalAlignment.Center,MaxWidth=560};confirmation.Child=prompt;
            prompt.Children.Add(PageTheme.Text("Restore these saves?",24));
            var text=PageTheme.Text(item+"\n\nThis replaces the entire save folder. A backup of your current saves is created first. Game files, settings and your ISO are kept.",14);text.Margin=new Thickness(0,16,0,20);prompt.Children.Add(text);
            var buttons=new WrapPanel();prompt.Children.Add(buttons);Button no=Button("Keep current saves","CancelSaveRestore"),yes=Button("Restore backup","ConfirmSaveRestore");PageTheme.Primary(yes);buttons.Children.Add(no);buttons.Children.Add(yes);
            body.IsEnabled=false;Ui.Show(confirmation,true);
            no.Click+=(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;};
            yes.Click+=async(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;await Run(async token=>{
                var progress=Reporter();await Task.Run(()=>SaveBackups.Restore(root,item.Id,progress,token,null,installationLockHeld),token);return "Saves restored. Your previous saves, if any, were backed up first.";
            });};
        }
        async Task Run(Func<CancellationToken,Task<string>> action)
        {
            if(preview){Report("Preview only. No save files were changed.");return;}if(stop!=null)return;
            stop=new CancellationTokenSource();body.IsEnabled=false;Ui.Show(Cancel,true);Cancel.IsEnabled=true;if(BusyChanged!=null)BusyChanged(true);
            try { Report("Preparing save operation…");Report(await action(stop.Token)); }
            catch(OperationCanceledException){Report("Cancelled. Your current saves were kept.");}
            catch(Exception ex){Report("Save operation: "+ex.Message);}
            finally {stop.Dispose();stop=null;body.IsEnabled=true;Ui.Show(Cancel,false);Reload();if(BusyChanged!=null)BusyChanged(false);}
        }
    }
}
