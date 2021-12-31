//
//  ViewController.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface ViewController : NSViewController

@property (weak) IBOutlet NSButton *btnExport;
@property (weak) IBOutlet NSTextField *txtboxOutput;
@property (weak) IBOutlet NSButton *btnToggleAll;
@property (weak) IBOutlet NSPopUpButton *popupBackup;
@property (weak) IBOutlet NSScrollView *sclApps;
@property (weak) IBOutlet NSTableView *tblApps;

@property (weak) IBOutlet NSButton *btnBackup;
@property (weak) IBOutlet NSButton *btnOutput;
@property (weak) IBOutlet NSTextField *lblBackup;
@property (weak) IBOutlet NSTextField *lblOutput;
@property (weak) IBOutlet NSTextField *lblApps;

@property (weak) IBOutlet NSProgressIndicator *progressBar;

// - (void)writeLog:(NSString *)log;


@end

