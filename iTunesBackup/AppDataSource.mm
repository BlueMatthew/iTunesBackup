//
//  SessionDataSource.m
//  WechatExporter
//
//  Created by Matthew on 2021/2/1.
//  Copyright © 2021 Matthew. All rights reserved.
//

#import "AppDataSource.h"

@implementation AppItem

- (NSComparisonResult)orgIndexCompare:(AppItem *)appItem ascending:(BOOL)ascending
{
    if (self.orgIndex < appItem.orgIndex)
        return NSOrderedAscending;
    else if (self.orgIndex > appItem.orgIndex)
        return NSOrderedDescending;
    
    return NSOrderedSame;
}

- (NSComparisonResult)nameCompare:(AppItem *)appItem ascending:(BOOL)ascending
{
    NSComparisonResult result = [self.name caseInsensitiveCompare:appItem.name];
    
    return result;
}

- (NSComparisonResult)bundleIdCompare:(AppItem *)appItem ascending:(BOOL)ascending
{
    NSComparisonResult result = [self.bundleId caseInsensitiveCompare:appItem.bundleId];
    
    return result;
}


@end

@interface AppDataSource()
{
    NSInteger m_indexOfSelectedUser;
    NSArray<AppItem *> *m_appItems;
}
@end

@implementation AppDataSource

- (instancetype)init
{
    if (self = [super init])
    {
        m_indexOfSelectedUser = -1;
        m_appItems = nil;
    }
    
    return self;
}

- (void)getSelectedApps:(NSMutableArray<NSString *> *)domains;
{
    
    for (AppItem *sessionItem in m_appItems)
    {
        if (sessionItem.checked)
        {
            NSString *domain = [NSString stringWithFormat:@"AppDomain-%@", sessionItem.bundleId];
            [domains addObject:domain];
        }
    }
}

- (void)loadData:(const std::vector<BackupManifest::AppInfo> *)apps
{
    // m_indexOfSelectedUser = indexOfSelectedUser;
    m_appItems = nil;
    if (apps == NULL)
    {
        return;
    }
    
    NSInteger orgIndex = 0;
    
    NSMutableArray<AppItem *> *appItems = [NSMutableArray<AppItem *> array];
    for (auto it = apps->cbegin(); it != apps->cend(); ++it)
    {
        
        AppItem *appItem = [[AppItem alloc] init];
        appItem.orgIndex = orgIndex;
        // sessionItem.userIndex = userIndex;
        appItem.checked = YES;
        appItem.name = [NSString stringWithUTF8String:it->name.c_str()];
        
        appItem.bundleId = [NSString stringWithUTF8String:it->bundleId.c_str()];
        
        
        [appItems addObject:appItem];
    
    }
    
    m_appItems = appItems;
}

- (void)checkAllApps:(BOOL)checked
{
    for (NSInteger idx = 0; idx < m_appItems.count; ++idx)
    {
        AppItem *appItem = [m_appItems objectAtIndex:idx];
        appItem.checked = checked;
    }
}

- (NSControlStateValue)updateCheckStateAtRow:(NSInteger)row
{
    if (row < m_appItems.count)
    {
        AppItem *sessionItem = [m_appItems objectAtIndex:row];
        if (sessionItem != nil)
        {
            sessionItem.checked = !sessionItem.checked;
        }
        
        BOOL checked = sessionItem.checked;
        BOOL allSame = YES;
        
        for (NSInteger idx = 0; idx < m_appItems.count; ++idx)
        {
            AppItem *sessionItem = [m_appItems objectAtIndex:idx];
            if (sessionItem.checked != checked)
            {
                allSame = NO;
                break;
            }
        }
        
        return allSame ? (checked ? NSControlStateValueOn : NSControlStateValueOff) : NSControlStateValueMixed;
    }
    
    return NSControlStateValueMixed;
}

- (void)clearSort
{
    m_appItems = [m_appItems sortedArrayUsingComparator: ^(AppItem *item1, AppItem *item2) {
        return [item1 orgIndexCompare:item2 ascending:YES];
    }];
}

- (void)sortOnName:(BOOL)ascending
{
    __block BOOL localAsc = ascending;
    
    m_appItems = [m_appItems sortedArrayUsingComparator: ^(AppItem *item1, AppItem *item2) {
        return [item1 nameCompare:item2 ascending:localAsc];
    }];
}

- (void)sortOnBundleId:(BOOL)ascending
{
    __block BOOL localAsc = ascending;
    
    m_appItems = [m_appItems sortedArrayUsingComparator: ^(AppItem *item1, AppItem *item2) {
        return [item1 bundleIdCompare:item2 ascending:localAsc];
    }];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return m_appItems.count;
}

/*
- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    return nil;
}
*/

- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray<NSSortDescriptor *> *)oldDescriptors
{
    if (tableView.sortDescriptors.count > 0)
    {
        NSSortDescriptor *sortDescriptor = [tableView.sortDescriptors objectAtIndex:0];
        if ([sortDescriptor.key isEqualToString:@"columnName"])
        {
            [self sortOnName:sortDescriptor.ascending];
        }
        else if ([sortDescriptor.key isEqualToString:@"columnBundleId"])
        {
            [self sortOnBundleId:sortDescriptor.ascending];
        }
    }
    else
    {
        [self clearSort];
    }
    
    [tableView reloadData];
}

- (void)bindCellView:(NSTableCellView *)cellView atRow:(NSInteger)row andColumnId:(NSString *)identifier
{
    AppItem *appItem = [m_appItems objectAtIndex:row];
    
    if ([identifier isEqualToString:@"columnCheck"])
    {
        NSButton *btn = (NSButton *)cellView.subviews.firstObject;
        if (btn)
        {
            btn.state = appItem.checked;
        }
    }
    else if([identifier isEqualToString:@"columnName"])
    {
        cellView.textField.stringValue = appItem.name;
    }
    else if([identifier isEqualToString:@"columnBundleId"])
    {
        cellView.textField.stringValue = appItem.bundleId;
    }
    
}


@end
