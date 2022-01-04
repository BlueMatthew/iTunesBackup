//
//  ViewController.m
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import "ViewController.h"
#include "AppDataSource.h"
#include "ITunesParser.h"
#import "BackupItem.h"
#include "Utils.h"
#include "FileSystem.h"

static bool handleFile(const std::string& output, const ITunesDb* iTunesDb, ITunesFile* file)
{
    std::string dest = combinePath(output, file->relativePath);
    if (file->isDir())
    {
        makeDirectory(dest);
    }
    else
    {
        copyFile(iTunesDb->getRealPath(file), dest, true);
    }

    unsigned int modifiedTime = ITunesDb::parseModifiedTime(file->blob);
    if (modifiedTime > 0)
    {
        updateFileTime(dest, modifiedTime);
    }

    return true;
}

@interface ViewController() <NSTableViewDelegate>
{
    std::vector<BackupManifest> m_manifests;
    NSInteger m_selectedIndex;
    
    AppDataSource   *m_dataSource;
}
@end

@implementation ViewController

- (instancetype)init
{
    if (self = [super init])
    {
        m_dataSource = [[AppDataSource alloc] init];
    }
    
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (self = [super initWithCoder:coder])
    {
        m_dataSource = [[AppDataSource alloc] init];
    }
    
    return self;
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil])
    {
        m_dataSource = [[AppDataSource alloc] init];
    }
    
    return self;
}

-(void)dealloc
{
    [self stopExporting];
}

- (void)stopExporting
{
    
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.popupBackup.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    self.btnBackup.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    
    self.lblBackup.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    self.lblOutput.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    self.lblApps.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    
    self.txtboxOutput.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    self.btnOutput.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    
    // self.popupUsers.autoresizingMask = NSViewMinYMargin;
    self.tblApps.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.sclApps.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    
    // self.sclViewLogs.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    
    self.progressBar.autoresizingMask = NSViewMaxYMargin;
    // self.btnCancel.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    // self.btnQuit.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    self.btnExport.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    self.btnExportWechat.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
    
    // sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, NULL);

    // [self.view.window center];
    
    m_selectedIndex = 0;
    
    [self.btnBackup setAction:@selector(btnBackupClicked:)];
    [self.btnOutput setAction:@selector(btnOutputClicked:)];
    [self.btnExport setAction:@selector(btnExportClicked:)];
    [self.btnExportWechat setAction:@selector(btnExportClicked:)];
    
    [self.popupBackup setTarget:self];
    [self.popupBackup setAction:@selector(handlePopupButton:)];
    
    [self.btnToggleAll setTarget:self];
    [self.btnToggleAll setAction:@selector(toggleAllApps:)];
    
    // btnBackupPicker.all
    // Do any additional setup after loading the view.
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    self.txtboxOutput.stringValue = [ViewController getDefaultOutputDir];
    
    NSURL *appSupport = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:nil];
    
    NSArray<NSString *> *components = @[[appSupport path], @"MobileSync", @"Backup"];
    NSString *backupDir = [NSString pathWithComponents:components];
    BOOL isDir = NO;
    if ([fileManager fileExistsAtPath:backupDir isDirectory:&isDir] && isDir)
    {
        NSString *backupDir = [NSString pathWithComponents:components];

        ManifestParser parser([backupDir UTF8String], true);
        std::vector<BackupManifest> manifests;
        if (parser.parse(manifests))
        {
            [self updateBackups:manifests];
        }
    }
    
    self.txtboxOutput.stringValue = [ViewController getLastOrDefaultOutputDir];
    
    NSRect frame = [self.tblApps.headerView headerRectOfColumn:0];
    NSRect btnFrame = self.btnToggleAll.frame;
    btnFrame.size.width = btnFrame.size.height;
    btnFrame.origin.x = (frame.size.width - btnFrame.size.width) / 2;
    btnFrame.origin.y = (frame.size.height - btnFrame.size.height) / 2;
    
    self.btnToggleAll.frame = btnFrame;
    
    [self.tblApps.headerView addSubview:self.btnToggleAll];
    for (NSTableColumn *tableColumn in self.tblApps.tableColumns)
    {
        NSSortDescriptor *sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:tableColumn.identifier ascending:YES selector:@selector(compare:)];
        [tableColumn setSortDescriptorPrototype:sortDescriptor];
    }
    
    self.tblApps.dataSource = m_dataSource;
    self.tblApps.delegate = self;
    
#ifndef NDEBUG
    
    /*
    NSString *workDir = [[NSFileManager defaultManager] currentDirectoryPath];
    
    workDir = [[NSBundle mainBundle] resourcePath];
    
    Exporter* exporter1 = new Exporter([workDir UTF8String], "/Users/matthew/Library/Application Support/MobileSync/Backup/11833774f1a5eed6ca84c0270417670f1483deae", "/Users/matthew/Documents/WxExp/OnlyWxBackup/Ext1", m_shell, m_logger);
    exporter1->run();
    delete exporter1;
    
    Exporter* exporter2 = new Exporter([workDir UTF8String], "/Users/matthew/Documents/WxExp/OnlyWxBackup/Backup/11833774f1a5eed6ca84c0270417670f1483deae", "/Users/matthew/Documents/WxExp/OnlyWxBackup/Ext2", m_shell, m_logger);
    exporter2->run();
    delete exporter2;
    */
    
#endif
}

+ (NSString *)getLastOrDefaultOutputDir
{
    NSString *outputDir = [[NSUserDefaults standardUserDefaults] stringForKey:@"OutputDir"];
    if (nil != outputDir && outputDir.length > 0)
    {
        return outputDir;
    }

    return [self getDefaultOutputDir];
}

+ (NSString *)getDefaultOutputDir
{
    NSMutableArray *components = [NSMutableArray array];
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    if (nil == paths && paths.count > 0)
    {
        [components addObject:[paths objectAtIndex:0]];
    }
    else
    {
        [components addObject:NSHomeDirectory()];
        [components addObject:@"Documents"];
    }
    [components addObject:@"iTunesBackup"];
    
    return [NSString pathWithComponents:components];
}

- (void)updateBackups:(const std::vector<BackupManifest>&) manifests
{
    int selectedIndex = -1;
    if (!manifests.empty())
    {
        // Add default backup folder
        for (std::vector<BackupManifest>::const_iterator it = manifests.cbegin(); it != manifests.cend(); ++it)
        {
            std::vector<BackupManifest>::const_iterator it2 = std::find(m_manifests.cbegin(), m_manifests.cend(), *it);
            if (it2 != m_manifests.cend())
            {
                if (selectedIndex == -1)
                {
                    selectedIndex = static_cast<int>(std::distance(it2, m_manifests.cbegin()));
                }
            }
            else
            {
                m_manifests.push_back(*it);
                if (selectedIndex == -1)
                {
                    selectedIndex = static_cast<int>(m_manifests.size() - 1);
                }
            }
        }
        
        // update
        [self.popupBackup removeAllItems];
        for (std::vector<BackupManifest>::const_iterator it = m_manifests.cbegin(); it != m_manifests.cend(); ++it)
        {
            std::string itemTitle = it->toString();
            NSString* item = [NSString stringWithUTF8String:itemTitle.c_str()];
            [self.popupBackup addItemWithTitle:item];
        }
        if (selectedIndex != -1 && selectedIndex < self.popupBackup.numberOfItems)
        {
            [self setPopupButton:self.popupBackup selectedItemAt:selectedIndex];
        }
    }
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}

- (void)setPopupButton:(NSPopUpButton *)popupButton selectedItemAt:(NSInteger)index
{
    [popupButton.menu performActionForItemAtIndex:index];
}

- (IBAction)handlePopupButton:(NSPopUpButton *)popupButton
{
    if (popupButton == self.popupBackup)
    {
        // m_usersAndSessions.clear();
        // [self.popupUsers removeAllItems];
        // self.txtViewLogs.string = @"";
        // Clear Users and Sessions
        // [m_dataSource loadData:&m_usersAndSessions withAllUsers:YES indexOfSelectedUser:-1];
        
        if (self.popupBackup.indexOfSelectedItem == -1 || self.popupBackup.indexOfSelectedItem >= m_manifests.size())
        {
            [m_dataSource loadData:NULL];
            [self.tblApps reloadData];
            return;
        }
        
        const BackupManifest& manifest = m_manifests[self.popupBackup.indexOfSelectedItem];
        if (manifest.isEncrypted())
        {
            [self msgBox:NSLocalizedString(@"err-encrypted-bkp-not-supported", @"")];
            [m_dataSource loadData:NULL];
            [self.tblApps reloadData];
            return;
        }
        
        const std::vector<BackupManifest::AppInfo>& apps = manifest.getApps();
        [m_dataSource loadData:&apps];
        [self.tblApps reloadData];
    }
}

- (void)btnBackupClicked:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = NO;
    panel.showsHiddenFiles = YES;
    
    [panel setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]]; // Set panel's default directory.

    [panel beginSheetModalForWindow:[self.view window] completionHandler: (^(NSInteger result){
        if (result == NSOKButton)
        {
            NSURL *backupUrl = panel.directoryURL;
            
            if ([backupUrl.absoluteString hasSuffix:@"/Backup"] || [backupUrl.absoluteString hasSuffix:@"/Backup/"])
            {
                ManifestParser parser([backupUrl.path UTF8String], true);
                std::vector<BackupManifest> manifests;
                if (parser.parse(manifests))
                {
                    [self updateBackups:manifests];
                }
            }
        }
    })];
}

- (void)btnOutputClicked:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = YES;
    panel.showsHiddenFiles = YES;
    
    NSString *outputPath = self.txtboxOutput.stringValue;
    if (nil == outputPath || [outputPath isEqualToString:@""])
    {
        [panel setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]]; // Set panel's default directory.
    }
    else
    {
        [panel setDirectoryURL:[NSURL fileURLWithPath:outputPath]];
    }
    
    [panel beginSheetModalForWindow:[self.view window] completionHandler: (^(NSInteger result){
        if (result == NSOKButton)
        {
            NSURL *url = panel.directoryURL;
            [ViewController setLastOutputDir:url.path];
            
            self.txtboxOutput.stringValue = url.path;
        }
    })];
}

- (void)btnExportClicked:(id)sender
{
    if (self.popupBackup.indexOfSelectedItem == -1 || self.popupBackup.indexOfSelectedItem >= m_manifests.size())
    {
        [self msgBox:NSLocalizedString(@"err-no-backup-dir", @"")];
        return;
    }
    std::string backup = m_manifests[self.popupBackup.indexOfSelectedItem].getPath();
    __block NSString *backupPath = [NSString stringWithUTF8String:backup.c_str()];
    BOOL isDir = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:backupPath isDirectory:&isDir] || !isDir)
    {
        [self msgBox:NSLocalizedString(@"err-backup-dir-doesnt-exist", @"")];
        return;
    }

    __block NSString *outputPath = [self.txtboxOutput.stringValue copy];

    if (nil == outputPath || [outputPath isEqualToString:@""])
    {
        [self msgBox:NSLocalizedString(@"err-no-output-dir", @"")];
        return;
    }
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:outputPath isDirectory:&isDir] || !isDir)
    {
        //
        [self msgBox:NSLocalizedString(@"err-output-dir-doesnt-exist", @"")];
        // self.txtboxOutput focus
        return;
    }
    __block NSMutableArray<NSString *> *domains = [NSMutableArray<NSString *> array];
    if (sender == self.btnExport)
    {
        [m_dataSource getSelectedApps:domains];
    }
    else
    {
        [domains addObject:@"AppDomain-com.tencent.xin"];
        [domains addObject:@"AppDomainGroup-group.com.tencent.xin"];
    }
    
    if (domains.count == 0)
    {
        [self msgBox:NSLocalizedString(@"err-no-selected-app", @"")];
        return;
    }
    
    [self disableExportButton];
    
    __block __weak __typeof__(self) weakSelf = self;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        __strong __typeof__(self) strongSelf = weakSelf;
        if (nil != strongSelf)
        {
            [strongSelf exportBackup:backupPath withApps:domains toOutput:outputPath];
            [strongSelf performSelectorOnMainThread:@selector(enableExportButton) withObject:nil waitUntilDone:NO];
        }
    });
}

- (void)exportBackup:(NSString *)backupPath withApps:(__kindof NSArray<NSString *> *)domains toOutput:(NSString *)outputPath
{
    std::string output([outputPath UTF8String]);
    std::string backup([backupPath UTF8String]);
    
    for(NSString *domain in domains)
    {
        std::string domainOutput = combinePath(output, [domain UTF8String]);
        makeDirectory(domainOutput);
        
        ITunesDb* iTunesDb = new ITunesDb(backup, "Manifest.db");
        if (iTunesDb->load([domain UTF8String]))
        {
            iTunesDb->enumFiles(std::bind(&handleFile, std::cref(domainOutput), std::placeholders::_1, std::placeholders::_2));
        }
        delete iTunesDb;
    }
}

- (void)exportWechatFiles:(NSString *)outputPath onBackup:(NSString *)backupPath
{
    NSArray<NSString *> *domains = @[@"AppDomain-com.tencent.xin", @"AppDomainGroup-group.com.tencent.xin"];
    
    [self exportBackup:backupPath withApps:domains toOutput:outputPath];
}

- (void)msgBox:(NSString *)msg
{
    __block NSString *localMsg = [NSString stringWithString:msg];
    __block NSString *title = [NSString stringWithString:self.title];
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.informativeText = localMsg;
        alert.messageText = title;
        [alert runModal];
    });
}

- (void)enableExportButton
{
    self.view.window.styleMask |= NSClosableWindowMask;
    [self.btnExport setEnabled:YES];
    [self.btnExportWechat setEnabled:YES];
    // [self.btnCancel setEnabled:NO];
    [self.popupBackup setEnabled:YES];
    [self.btnOutput setEnabled:YES];
    [self.btnBackup setEnabled:YES];
    [self.progressBar stopAnimation:nil];
}

- (void)disableExportButton
{
    self.view.window.styleMask &= ~NSClosableWindowMask;
    [self.popupBackup setEnabled:NO];
    [self.btnOutput setEnabled:NO];
    [self.btnBackup setEnabled:NO];
    [self.btnExport setEnabled:NO];
    [self.btnExportWechat setEnabled:NO];
    // [self.btnCancel setEnabled:YES];
    [self.progressBar startAnimation:nil];
}

- (void)checkButtonTapped:(id)sender
{
    NSButton *btn = (NSButton *)sender;
    NSControlStateValue state = [m_dataSource updateCheckStateAtRow:btn.tag];
    self.btnToggleAll.state = state;
}

- (void)tableViewDoubleClick:(id)sender
{
    NSTableView *tableView = (NSTableView *)sender;
    if (1 != tableView.clickedColumn)
    {
        return;
    }
    NSView *view = [tableView viewAtColumn:tableView.clickedColumn row:tableView.clickedRow makeIfNecessary:NO];
    if (nil == view)
    {
        return;
    }
    NSTextField *textField = (NSTextField *)view.subviews.firstObject;
    if (nil != textField && nil != textField.stringValue)
    {
        [[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
        [[NSPasteboard generalPasteboard] setString:textField.stringValue forType:NSStringPboardType];
    }
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSTableCellView *cellView = [tableView makeViewWithIdentifier:[tableColumn identifier] owner:self];
    if ([[tableColumn identifier] isEqualToString:@"columnCheck"])
    {
        NSButton *btn = (NSButton *)cellView.subviews.firstObject;
        if (btn)
        {
            [btn setTarget:self];
            [btn setAction:@selector(checkButtonTapped:)];
            btn.tag = row;
        }
    }
    
    [m_dataSource bindCellView:cellView atRow:row andColumnId:[tableColumn identifier]];
    return cellView;
}

- (void)toggleAllApps:(id)sender
{
    NSButton *btn = (NSButton *)sender;
    if (btn.state == NSControlStateValueMixed)
    {
        [self.btnToggleAll setNextState];
    }
        
    if (btn.state == NSControlStateValueOn)
    {
        [m_dataSource checkAllApps:YES];
    }
    else if (btn.state == NSControlStateValueOff)
    {
        [m_dataSource checkAllApps:NO];
    }
    
    [self.tblApps reloadData];
}

- (void)tableViewColumnDidResize:(NSNotification *)notification
{
    if ([notification.name isEqualToString:NSTableViewColumnDidResizeNotification])
    {
        NSTableView *tableView = (NSTableView *)notification.object;
        NSTableColumn *column = [notification.userInfo objectForKey:@"NSTableColumn"];
        
        if (tableView == self.tblApps && [column.identifier isEqualToString:@"columnCheck"])
        {
            NSRect frame = [self.tblApps.headerView headerRectOfColumn:0];
            NSRect btnFrame = self.btnToggleAll.frame;
            btnFrame.origin.x = (frame.size.width - btnFrame.size.width) / 2;
            btnFrame.origin.y = (frame.size.height - btnFrame.size.height) / 2;
            
            self.btnToggleAll.frame = btnFrame;
        }
        
    }
}

+ (void)setLastOutputDir:(NSString *)outputDir
{
    [[NSUserDefaults standardUserDefaults] setObject:outputDir forKey:@"OutputDir"];
}

@end
