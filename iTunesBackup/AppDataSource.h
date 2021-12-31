//
//  SessionDataSource.h
//  WechatExporter
//
//  Created by Matthew on 2021/2/1.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef SessionDataSource_h
#define SessionDataSource_h

#import <Cocoa/Cocoa.h>
#include <vector>
#include <set>
#include <utility>

#include "ITunesParser.h"

@interface AppItem : NSObject

@property (assign) NSInteger orgIndex;
// @property (assign) NSInteger userIndex;
@property (assign) BOOL checked;
@property (strong) NSString *name;
@property (strong) NSString *bundleId;

- (NSComparisonResult)orgIndexCompare:(AppItem *)appItem ascending:(BOOL)ascending;
- (NSComparisonResult)nameCompare:(AppItem *)appItem ascending:(BOOL)ascending;
- (NSComparisonResult)bundleIdCompare:(AppItem *)appItem ascending:(BOOL)ascending;
// - (NSComparisonResult)userIndexCompare:(AppItem *)appItem ascending:(BOOL)ascending;

@end

@interface AppDataSource : NSObject<NSTableViewDataSource>

- (void)loadData:(const std::vector<BackupManifest::AppInfo> *)apps;
- (void)getSelectedApps:(NSMutableArray<NSString *> *)domains;

- (void)bindCellView:(NSTableCellView *)cellView atRow:(NSInteger)row andColumnId:(NSString *)identifier;
- (NSControlStateValue)updateCheckStateAtRow:(NSInteger)row;
- (void)checkAllApps:(BOOL)checked;

@end

#endif /* SessionDataSource_h */
