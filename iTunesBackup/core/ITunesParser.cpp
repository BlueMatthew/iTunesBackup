//
//  ITunesParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "ITunesParser.h"
#include <stdio.h>
#include <map>
#include <set>
#include <sys/types.h>
#include <sqlite3.h>
#include <algorithm>
#include <plist/plist.h>

#ifndef NDEBUG
#include <assert.h>
#endif
#include <sys/stat.h>
#if defined(_WIN32)
// #define S_IFMT          0170000         /* [XSI] type of file mask */
// #define S_IFDIR         0040000         /* [XSI] directory */
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)     /* directory */

#else
#include <unistd.h>
#endif

#include "MbdbReader.h"
#include "Utils.h"
#include "FileSystem.h"

inline std::string getPlistStringValue(plist_t node)
{
    std::string value;
    
    if (NULL != node)
    {
        uint64_t length = 0;
        const char* ptr = plist_get_string_ptr(node, &length);
        if (length > 0)
        {
            value.assign(ptr, length);
        }
    }
    
    return value;
}

inline std::string getPlistStringValue(plist_t node, const char* key)
{
    std::string value;
    
    if (NULL != node)
    {
        plist_t subNode = plist_dict_get_item(node, key);
        if (NULL != subNode)
        {
            uint64_t length = 0;
            const char* ptr = plist_get_string_ptr(subNode, &length);
            if (length > 0)
            {
                value.assign(ptr, length);
            }
        }
    }
    
    return value;
}

struct __string_less
{
    // _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX11
    bool operator()(const std::string& __x, const std::string& __y) const {return __x < __y;}
    bool operator()(const std::pair<std::string, std::string>& __x, const std::string& __y) const {return __x.first < __y;}
    bool operator()(const ITunesFile* __x, const std::string& __y) const {return __x->relativePath < __y;}
    bool operator()(const ITunesFile* __x, const ITunesFile* __y) const {return __x->relativePath < __y->relativePath;}
};

class SqliteITunesFileEnumerator : public ITunesDb::ITunesFileEnumerator
{
public:
    SqliteITunesFileEnumerator(const std::string& dbPath, const std::string& domain, bool onlyFile) : m_db(NULL), m_stmt(NULL), m_onlyFile(onlyFile)
    {
        int rc = openSqlite3Database(dbPath, &m_db);
        if (rc != SQLITE_OK)
        {
            // printf("Open database failed!");
            closeDb();
            return;
        }

        sqlite3_exec(m_db, "PRAGMA mmap_size=2097152;", NULL, NULL, NULL); // 8M:8388608  2M 2097152
        sqlite3_exec(m_db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
        
        std::string sql = "SELECT fileID,relativePath,flags,file FROM Files";
        if (domain.size() > 0)
        {
            sql += " WHERE domain=?";
        }

        rc = sqlite3_prepare_v2(m_db, sql.c_str(), (int)(sql.size()), &m_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            std::string error = sqlite3_errmsg(m_db);
            closeDb();
            return;
        }
        
        if (domain.size() > 0)
        {
            rc = sqlite3_bind_text(m_stmt, 1, domain.c_str(), (int)(domain.size()), NULL);
            if (rc != SQLITE_OK)
            {
                finalizeStmt();
                closeDb();
                return;
            }
        }
    }
    
    virtual bool isInvalid() const
    {
        return NULL != m_db && NULL != m_stmt;
    }
    
    virtual bool nextFile(ITunesFile& file)
    {
        while (sqlite3_step(m_stmt) == SQLITE_ROW)
        {
            int flags = sqlite3_column_int(m_stmt, 2);
            if (m_onlyFile && flags == 2)
            {
                // Putting flags=1 into sql causes sqlite3 to use index of flags instead of domain and don't know why...
                // So filter the directory with the code
                continue;
            }
            
            const char *relativePath = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, 1));
            if (NULL != relativePath)
            {
                file.relativePath = relativePath;
            }
            else
            {
                file.relativePath.clear();
            }
            const char *fileId = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, 0));
            if (NULL != fileId)
            {
                file.fileId = fileId;
            }
            else
            {
                file.fileId.clear();
            }

            file.flags = static_cast<unsigned int>(flags);
            // Files
            const unsigned char *blob = reinterpret_cast<const unsigned char*>(sqlite3_column_blob(m_stmt, 3));
            int blobBytes = sqlite3_column_bytes(m_stmt, 3);
            file.blob.clear();
            if (blobBytes > 0 && NULL != blob)
            {
                std::vector<unsigned char> blobVector(blob, blob + blobBytes);
                file.blob.insert(file.blob.end(), blob, blob + blobBytes);
            }

            break;
        }

        return false;
    }
    
    virtual ~SqliteITunesFileEnumerator()
    {
        finalizeStmt();
        closeDb();
    }
    
private:
    void closeDb()
    {
        if (NULL != m_db)
        {
            sqlite3_close(m_db);
            m_db = NULL;
        }
    }
    
    void finalizeStmt()
    {
        if (NULL != m_stmt)
        {
            sqlite3_finalize(m_stmt);
            m_stmt = NULL;
        }
    }
private:
    sqlite3*        m_db;
    sqlite3_stmt*   m_stmt;
    
    bool m_onlyFile;
};

class MbdbITunesFileEnumerator : public ITunesDb::ITunesFileEnumerator
{
public:
    MbdbITunesFileEnumerator(const std::string& dbPath, const std::string& domain, bool onlyFile) : m_valid(false), m_domain(domain), m_onlyFile(onlyFile)
    {
        std::memset(m_fixedData, 0, 40);
        if (!m_reader.open(dbPath))
        {
            return;
        }
        
        m_valid = true;
    }
    
    virtual bool isInvalid() const
    {
        return m_valid;
    }
    
    virtual bool nextFile(ITunesFile& file)
    {
        std::string domainInFile;
        std::string path;
        std::string linkTarget;
        std::string dataHash;
        std::string alwaysNull;
        unsigned short fileMode = 0;
        bool isDir = false;
        bool skipped = false;
        
        // bool hasFilter = (bool)m_loadingFilter;

        while (m_reader.hasMoreData())
        {
            if (!m_reader.read(domainInFile))
            {
                break;
            }
            
            skipped = false;
            if (!m_domain.empty() && m_domain != domainInFile)
            {
                skipped = true;
            }
            
            if (skipped)
            {
                // will skip it
                m_reader.skipString();    // path
                m_reader.skipString();    // linkTarget
                m_reader.skipString();    // dataHash
                m_reader.skipString();    // alwaysNull;
                
                m_reader.read(m_fixedData, 40);
                int propertyCount = m_fixedData[39];

                for (int j = 0; j < propertyCount; ++j)
                {
                    m_reader.skipString();
                    m_reader.skipString();
                }
            }
            else
            {
                m_reader.read(path);
                m_reader.read(linkTarget);
                m_reader.readD(dataHash);
                m_reader.readD(alwaysNull);
                
                m_reader.read(m_fixedData, 40);

                fileMode = (m_fixedData[0] << 8) | m_fixedData[1];
                
                isDir = S_ISDIR(fileMode);
                
                // unsigned char flags = fixedData[38];
                
                if (m_onlyFile && isDir)
                {
                    skipped = true;
                }
                
                unsigned int aTime = bigEndianToNative(*((uint32_t *)(m_fixedData + 18)));
                unsigned int bTime = bigEndianToNative(*((uint32_t *)(m_fixedData + 22)));
                // unsigned int cTime = bigEndianToNative(*((uint32_t *)(m_fixedData + 26)));
                
                file.size = bigEndianToNative(*((int64_t *)(m_fixedData + 30)));
                
                int propertyCount = m_fixedData[39];
                
                for (int j = 0; j < propertyCount; ++j)
                {
                    if (skipped)
                    {
                        m_reader.skipString(); // name
                        m_reader.skipString(); // value
                    }
                    else
                    {
                        std::string name;
                        std::string value;
                        
                        m_reader.read(name);
                        m_reader.read(value);
                    }
                }
                
                if (!skipped)
                {
                    file.relativePath = path;
                    file.fileId = sha1(domainInFile + "-" + path);
                    file.flags = isDir ? 2 : 1;
                    file.modifiedTime = aTime != 0 ? aTime : bTime;
                    // file.size =
                    
                    return true;
                }
                
            }
        }
        
        return false;
    }
    
    virtual ~MbdbITunesFileEnumerator()
    {
    }

private:
    MbdbReader      m_reader;
    bool            m_valid;
    std::string     m_domain;
    bool            m_onlyFile;
    unsigned char   m_fixedData[40];
};


ITunesDb::ITunesDb(const std::string& rootPath, const std::string& manifestFileName) : m_isMbdb(false), m_rootPath(rootPath), m_manifestFileName(manifestFileName)
{
    std::replace(m_rootPath.begin(), m_rootPath.end(), ALT_DIR_SEP, DIR_SEP);
    
    if (!endsWith(m_rootPath, DIR_SEP))
    {
        m_rootPath += DIR_SEP;
    }
}

ITunesDb::~ITunesDb()
{
    for (std::vector<ITunesFile *>::iterator it = m_files.begin(); it != m_files.end(); ++it)
    {
        delete *it;
    }
    m_files.clear();
}

bool ITunesDb::load()
{
    return load("", false);
}

bool ITunesDb::load(const std::string& domain)
{
    return load(domain, false);
}

bool ITunesDb::load(const std::string& domain, bool onlyFile)
{
    m_version.clear();
    BackupManifest manifest;
    if (ManifestParser::parseInfoPlist(m_rootPath, manifest, false))
    {
        m_version = manifest.getITunesVersion();
        m_iOSVersion = manifest.getIOSVersion();
    }
    
    std::string dbPath = combinePath(m_rootPath, "Manifest.mbdb");
    if (existsFile(dbPath))
    {
        m_isMbdb = true;
        return loadMbdb(domain, onlyFile);
    }
    
    m_isMbdb = false;
    dbPath = combinePath(m_rootPath, "Manifest.db");
    
    sqlite3 *db = NULL;
    int rc = openSqlite3Database(dbPath, &db);
    if (rc != SQLITE_OK)
    {
        // printf("Open database failed!");
        sqlite3_close(db);
        return false;
    }

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: start.....%s\r\n", getTimestampString(false, true).c_str());
#endif

    sqlite3_exec(db, "PRAGMA mmap_size=2097152;", NULL, NULL, NULL); // 8M:8388608  2M 2097152
    sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
    
    std::string sql = "SELECT fileID,relativePath,flags,file FROM Files";
    if (domain.size() > 0)
    {
        sql += " WHERE domain=?";
    }
    
    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    
    if (domain.size() > 0)
    {
        rc = sqlite3_bind_text(stmt, 1, domain.c_str(), (int)(domain.size()), NULL);
        if (rc != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
    }
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: %s sql=%s, domain=%s\r\n", getTimestampString(false, true).c_str(), sql.c_str(), domain.c_str());
#endif
    
    bool hasFilter = (bool)m_loadingFilter;
    
    m_files.reserve(2048);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int flags = sqlite3_column_int(stmt, 2);
        if (onlyFile && flags == 2)
        {
            // Putting flags=1 into sql causes sqlite3 to use index of flags instead of domain and don't know why...
            // So filter the directory with the code
            continue;
        }
        
        const char *relativePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (hasFilter && !m_loadingFilter(relativePath, flags))
        {
            continue;
        }
        
        ITunesFile *file = new ITunesFile();
        const char *fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL != fileId)
        {
            file->fileId = fileId;
        }
        
        if (NULL != relativePath)
        {
            file->relativePath = relativePath;
        }
        file->flags = static_cast<unsigned int>(flags);
        if (flags == 1)
        {
            // Files
            int blobBytes = sqlite3_column_bytes(stmt, 3);
            const unsigned char *blob = reinterpret_cast<const unsigned char*>(sqlite3_column_blob(stmt, 3));
            if (blobBytes > 0 && NULL != blob)
            {
                std::vector<unsigned char> blobVector(blob, blob + blobBytes);
                file->blob.swap(blobVector);
            }
        }
        
        m_files.push_back(file);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: end.....%s, size=%lu\r\n", getTimestampString(false, true).c_str(), m_files.size());
#endif
    
    std::sort(m_files.begin(), m_files.end(), __string_less());
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: after sort.....%s\r\n", getTimestampString(false, true).c_str());
#endif
    return true;
}

bool ITunesDb::loadMbdb(const std::string& domain, bool onlyFile)
{
    MbdbReader reader;
    if (!reader.open(combinePath(m_rootPath, "Manifest.mbdb")))
    {
        return false;
    }
    
    // unsigned char mdbxBuffer[26];           // buffer for .mbdx record
    unsigned char fixedData[40] = { 0 };    // buffer for the fixed part of .mbdb record
    // SHA1CryptoServiceProvider hasher = new SHA1CryptoServiceProvider();

    // System.DateTime unixEpoch = new System.DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc);

    std::string domainInFile;
    std::string path;
    std::string linkTarget;
    std::string dataHash;
    std::string alwaysNull;
    unsigned short fileMode = 0;
    bool isDir = false;
    bool skipped = false;
    
    bool hasFilter = (bool)m_loadingFilter;

    while (reader.hasMoreData())
    {
        if (!reader.read(domainInFile))
        {
            break;
        }
        
        skipped = false;
        if (!domain.empty() && domain != domainInFile)
        {
            skipped = true;
        }
        
        if (skipped)
        {
            // will skip it
            reader.skipString();    // path
            reader.skipString();    // linkTarget
            reader.skipString();    // dataHash
            reader.skipString();    // alwaysNull;
            
            reader.read(fixedData, 40);
            int propertyCount = fixedData[39];

            for (int j = 0; j < propertyCount; ++j)
            {
                reader.skipString();
                reader.skipString();
            }
        }
        else
        {
            reader.read(path);
            reader.read(linkTarget);
            reader.readD(dataHash);
            reader.readD(alwaysNull);
            
            reader.read(fixedData, 40);

            fileMode = (fixedData[0] << 8) | fixedData[1];
            
            isDir = S_ISDIR(fileMode);
            
            // unsigned char flags = fixedData[38];
            
            if (onlyFile && isDir)
            {
                skipped = true;
            }
            
            if (hasFilter && !m_loadingFilter(path.c_str(), (isDir ? 2 : 1)))
            {
                skipped = true;
            }
            
            unsigned int aTime = GetBigEndianInteger(fixedData, 18);
            unsigned int bTime = GetBigEndianInteger(fixedData, 22);
            // unsigned int cTime = GetBigEndianInteger(fixedData, 26);
            
            int propertyCount = fixedData[39];
            
            // rec.Properties = new MBFileRecord.Property[rec.PropertyCount];
            for (int j = 0; j < propertyCount; ++j)
            {
                if (skipped)
                {
                    reader.skipString(); // name
                    reader.skipString(); // value
                }
                else
                {
                    std::string name;
                    std::string value;
                    
                    reader.read(name);
                    reader.read(value);
                }
                
            }
            /*
            StringBuilder fileName = new StringBuilder();
            byte[] fb = hasher.ComputeHash(ASCIIEncoding.UTF8.GetBytes(rec.Domain + "-" + rec.Path));
            for (int k = 0; k < fb.Length; k++)
            {
                fileName.Append(fb[k].ToString("x2"));
            }

            rec.key = fileName.ToString();
             */
            
            if (!skipped)
            {
                ITunesFile *file = new ITunesFile();
                file->relativePath = path;
                file->fileId = sha1(domainInFile + "-" + path);
                file->flags = isDir ? 2 : 1;
                file->modifiedTime = aTime != 0 ? aTime : bTime;
                
                m_files.push_back(file);
            }
            
        }
        
        
    }
    
    std::sort(m_files.begin(), m_files.end(), __string_less());

    return true;
}

bool ITunesDb::copy(const std::string& destPath, const std::string& backupId, std::vector<std::string>& domains) const
{
    std::string dbPath = combinePath(m_rootPath, "Manifest.mbdb");
    if (existsFile(dbPath))
    {
        return copyMbdb(destPath, backupId, domains);
    }
    
    std::string destBackupPath = backupId.empty() ? combinePath(destPath, "Backup") : combinePath(destPath, "Backup", backupId);
    if (!existsDirectory(destBackupPath))
    {
        makeDirectory(destBackupPath);
    }
    
    // Copy control files
    const char* files[] = {"Info.plist", "Manifest.db", "Manifest.plist", "Status.plist"};
    for (int idx = 0; idx < sizeof(files) / sizeof(const char *); ++idx)
    {
        ::copyFile(combinePath(m_rootPath, files[idx]), combinePath(destBackupPath, files[idx]));
    }

    dbPath = combinePath(destBackupPath, "Manifest.db");
    
    sqlite3 *db = NULL;
    int rc = openSqlite3Database(dbPath, &db, false);
    if (rc != SQLITE_OK)
    {
        // printf("Open database failed!");
        sqlite3_close(db);
        return false;
    }

    sqlite3_exec(db, "PRAGMA mmap_size=2097152;", NULL, NULL, NULL); // 8M:8388608  2M 2097152
    sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
    
    std::string sql = "DELETE FROM Files";
    
    std::vector<std::string> appDomains;
    std::vector<std::string> appPluginDomains;
    if (domains.size() > 0)
    {
        appDomains.reserve(domains.size() * 2);
        appPluginDomains.reserve(domains.size());
        
        for (std::vector<std::string>::const_iterator it = domains.cbegin(); it != domains.cend(); ++it)
        {
            appDomains.push_back("AppDomain-" + *it);
            appDomains.push_back("AppDomainGroup-group." + *it);
            
            appPluginDomains.push_back("AppDomainPlugin-" + *it + ".%");
        }
        sql += " WHERE ";
        // domain=?";
        std::vector<std::string> conditions(appDomains.size(), "domain<>?");
        conditions.reserve(conditions.size() + appPluginDomains.size());
        std::vector<std::string> conditions2(appDomains.size(), "(domain NOT LIKE )");
        
        sql += join(conditions, " AND ");
    }
    
    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }

    int idx = 1;
    for (std::vector<std::string>::const_iterator it = domains.cbegin(); it != domains.cend(); ++it, ++idx)
    {
        rc = sqlite3_bind_text(stmt, idx, (*it).c_str(), (int)((*it).size()), NULL);
        if (rc != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
    }
    
#if !defined(NDEBUG)
#ifdef __APPLE__
    if (__builtin_available(macOS 10.12, *))
    {
#endif
		char *expandedSql = sqlite3_expanded_sql(stmt);
        printf("PERF: %s sql=%s\r\n", getTimestampString(false, true).c_str(), sqlite3_expanded_sql(stmt));
#ifdef __APPLE__
    }
#endif
#endif
    
    if ((rc = sqlite3_step(stmt)) != SQLITE_DONE)
    {
#ifndef NDEBUG
		const char *errMsg = sqlite3_errmsg(db);
        m_lastError = std::string(errMsg);
#endif
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }
    
    sqlite3_finalize(stmt);
    
    sqlite3_exec(db, "VACUUM;", NULL, NULL, NULL);
    
    std::set<std::string> subFolders;
    sql = "SELECT fileID,flags FROM Files";
    stmt = NULL;
    std::string fileId;
    std::string subFolder;
    std::string subPath;
    std::string srcFilePath;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL == str)
        {
            continue;
        }
        fileId = str;
        subFolder = fileId.substr(0, 2);
        
        srcFilePath = combinePath(m_rootPath, subFolder, fileId);
        if (!existsFile(srcFilePath))
        {
#ifndef NDEBUG
			int flags = sqlite3_column_int(stmt, 1);
			if (flags == 1)
			{
				assert(!"Source file not exists.");
			}
#endif
            continue;
        }

        subPath = combinePath(destBackupPath, subFolder);
        if (subFolders.find(subFolder) == subFolders.cend())
        {
            if (!existsDirectory(subPath))
            {
                makeDirectory(subPath);
            }
            subFolders.insert(subFolder);
        }
        bool ret = ::copyFile(srcFilePath, combinePath(subPath, fileId));
#ifndef NDEBUG
		if (!ret)
		{
			std::string msg = "Failed to copy file" + combinePath(subPath, fileId);
			assert(!msg.c_str());
		}
#endif
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return true;
}

bool ITunesDb::copyMbdb(const std::string& destPath, const std::string& backupId, std::vector<std::string>& domains) const
{
    std::string destBackupPath = backupId.empty() ? combinePath(destPath, "Backup") : combinePath(destPath, "Backup", backupId);
    if (!existsDirectory(destBackupPath))
    {
        makeDirectory(destBackupPath);
    }
    
    // Copy control files
    const char* files[] = {"Info.plist", "Manifest.mbdb", "Manifest.plist", "Status.plist"};
    for (int idx = 0; idx < sizeof(files) / sizeof(const char *); ++idx)
    {
        ::copyFile(combinePath(m_rootPath, files[idx]), combinePath(destBackupPath, files[idx]));
    }
    
    MbdbReader reader;
    if (!reader.open(combinePath(destBackupPath, "Manifest.mbdb")))
    {
        return false;
    }
    
    // unsigned char mdbxBuffer[26];           // buffer for .mbdx record
    unsigned char fixedData[40] = { 0 };    // buffer for the fixed part of .mbdb record
    // SHA1CryptoServiceProvider hasher = new SHA1CryptoServiceProvider();

    // System.DateTime unixEpoch = new System.DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc);

    std::string domainInFile;
    std::string path;
    std::string fileId;
    bool skipped = false;
    
    bool hasFilter = (bool)m_loadingFilter;

    while (reader.hasMoreData())
    {
        if (!reader.read(domainInFile))
        {
            break;
        }
        
        skipped = false;
        if (!domains.empty() && (std::find(domains.cbegin(), domains.cend(), domainInFile) != domains.cend()))
        {
            skipped = true;
        }
        
        if (skipped)
        {
            // will skip it
            reader.skipString();    // path
        }
        else
        {
            reader.read(path);
            
            fileId = sha1(domainInFile + "-" + path);
            ::copyFile(combinePath(m_rootPath, fileId), combinePath(destBackupPath, fileId));
        }
        
        reader.skipString();    // linkTarget
        reader.skipString();    // dataHash
        reader.skipString();    // alwaysNull
        
        reader.read(fixedData, 40);
        int propertyCount = fixedData[39];

        for (int j = 0; j < propertyCount; ++j)
        {
            reader.skipString();
            reader.skipString();
        }
        
    }
    
    std::sort(m_files.begin(), m_files.end(), __string_less());

    return true;
}

unsigned int ITunesDb::parseModifiedTime(const std::vector<unsigned char>& data)
{
    if (data.empty())
    {
        return 0;
    }
    uint64_t val = 0;
    plist_t node = NULL;
    plist_from_memory(reinterpret_cast<const char *>(&data[0]), static_cast<uint32_t>(data.size()), &node);
    if (NULL != node)
    {
        plist_t lastModified = plist_access_path(node, 3, "$objects", 1, "LastModified");
        if (NULL != lastModified)
        {
            plist_get_uint_val(lastModified, &val);
        }
        
        plist_free(node);
    }

    return static_cast<unsigned int>(val);
}

bool ITunesDb::parseFileInfo(const ITunesFile* file)
{
    if (NULL == file || file->blob.empty())
    {
        return false;
    }
    
    if (file->blobParsed)
    {
        return true;
    }
    
    file->blobParsed = true;
    
    uint64_t val = 0;
    plist_t node = NULL;
    plist_from_memory(reinterpret_cast<const char *>(&file->blob[0]), static_cast<uint32_t>(file->blob.size()), &node);
    if (NULL != node)
    {
        plist_t lastModifiedNode = plist_access_path(node, 3, "$objects", 1, "LastModified");
        if (NULL != lastModifiedNode)
        {
            plist_get_uint_val(lastModifiedNode, &val);
            file->modifiedTime = (unsigned int)val;
        }
        
        plist_t sizeNode = plist_access_path(node, 3, "$objects", 1, "Size");
        if (NULL != sizeNode)
        {
            val = 0;
            plist_get_uint_val(sizeNode, &val);
            file->size = val;
        }
        
        plist_free(node);
        return true;
    }
    
    return false;
}

std::string ITunesDb::findFileId(const std::string& relativePath) const
{
    const ITunesFile* file = findITunesFile(relativePath);
    
    if (NULL == file)
    {
        return std::string();
    }
    return file->fileId;
}

const ITunesFile* ITunesDb::findITunesFile(const std::string& relativePath) const
{
    std::string formatedPath = relativePath;
    std::replace(formatedPath.begin(), formatedPath.end(), '\\', '/');

    typename std::vector<ITunesFile *>::iterator it = std::lower_bound(m_files.begin(), m_files.end(), formatedPath, __string_less());
    
    if (it == m_files.end() || (*it)->relativePath != formatedPath)
    {
        return NULL;
    }
    return *it;
}

std::string ITunesDb::fileIdToRealPath(const std::string& fileId) const
{
    if (!fileId.empty())
    {
        return m_isMbdb ? combinePath(m_rootPath, fileId) : combinePath(m_rootPath, fileId.substr(0, 2), fileId);
    }
    
    return std::string();
}

std::string ITunesDb::getRealPath(const ITunesFile& file) const
{
    return fileIdToRealPath(file.fileId);
}

std::string ITunesDb::getRealPath(const ITunesFile* file) const
{
    return fileIdToRealPath(file->fileId);
}

std::string ITunesDb::findRealPath(const std::string& relativePath) const
{
    std::string fieldId = findFileId(relativePath);
    return fileIdToRealPath(fieldId);
}

bool ITunesDb::copyFile(const std::string& vpath, const std::string& dest, bool overwrite/* = false*/) const
{
    std::string destPath = normalizePath(dest);
    if (!overwrite && existsFile(destPath))
    {
        return true;
    }
    
    const ITunesFile* file = findITunesFile(vpath);
    if (NULL != file)
    {
        std::string srcPath = getRealPath(*file);
        if (!srcPath.empty())
        {
            normalizePath(srcPath);
            bool result = ::copyFile(srcPath, destPath, true);
            if (result)
            {
                updateFileTime(dest, ITunesDb::parseModifiedTime(file->blob));
            }
            return result;
        }
    }
    
    return false;
}

bool ITunesDb::copyFile(const std::string& vpath, const std::string& destPath, const std::string& destFileName, bool overwrite/* = false*/) const
{
    std::string destFullPath = normalizePath(combinePath(destPath, destFileName));
    if (!overwrite && existsFile(destFullPath))
    {
        return true;
    }
    
    const ITunesFile* file = findITunesFile(vpath);
    if (NULL != file)
    {
        std::string srcPath = getRealPath(*file);
        if (!srcPath.empty())
        {
            normalizePath(srcPath);
            if (!existsDirectory(destPath))
            {
                makeDirectory(destPath);
            }
            bool result = ::copyFile(srcPath, destFullPath, true);
            if (result)
            {
                if (file->modifiedTime != 0)
                {
                    updateFileTime(destFullPath, static_cast<time_t>(file->modifiedTime));
                }
                else if (!file->blob.empty())
                {
                    updateFileTime(destFullPath, ITunesDb::parseModifiedTime(file->blob));
                }
            }
            return result;
        }
    }
    
    return false;
}

ManifestParser::ManifestParser(const std::string& manifestPath, bool incudingApps) : m_manifestPath(manifestPath), m_incudingApps(incudingApps)
{
}

std::string ManifestParser::getLastError() const
{
	return m_lastError;
}

bool ManifestParser::parse(std::vector<BackupManifest>& manifests) const
{
    bool res = false;
    
    std::string path = normalizePath(m_manifestPath);
    if (endsWith(path, normalizePath("/MobileSync")) || endsWith(path, normalizePath("/MobileSync/")) || isValidMobileSync(path))
    {
        path = combinePath(path, "Backup");
        res = parseDirectory(path, manifests);
    }
    else if (isValidBackupItem(path))
    {
        BackupManifest manifest;
        if (parse(path, manifest) && manifest.isValid())
        {
            manifests.push_back(manifest);
            res = true;
        }
    }
    else
    {
        // Assume the directory is ../../Backup/../
        res = parseDirectory(path, manifests);
    }
    
    return res;
}

bool ManifestParser::parseDirectory(const std::string& path, std::vector<BackupManifest>& manifests) const
{
    std::vector<std::string> subDirectories;
    if (!listSubDirectories(path, subDirectories))
    {
#ifndef NDEBUG
#endif
		m_lastError += "Failed to list subfolder in:" + path + "\r\n";
        return false;
    }
    
    bool res = false;
    for (std::vector<std::string>::const_iterator it = subDirectories.cbegin(); it != subDirectories.cend(); ++it)
    {
        std::string backupPath = combinePath(path, *it);
		if (!isValidBackupItem(backupPath))
		{
			continue;
		}
        
        BackupManifest manifest;
        manifest.setBackupId(*it);
        if (parse(backupPath, manifest) && manifest.isValid())
        {
            manifests.push_back(manifest);
            res = true;
        }
    }

	if (!res)
	{
		m_lastError += "No valid backup id found in:" + path + "\r\n";
	}

    return res;
}

bool ManifestParser::isValidBackupItem(const std::string& path) const
{
    std::string fileName = combinePath(path, "Info.plist");
    if (!existsFile(fileName))
    {
        m_lastError += "Info.plist not found\r\n";
        return false;
    }

    fileName = combinePath(path, "Manifest.plist");
    if (!existsFile(fileName))
    {
        m_lastError += "Manifest.plist not found\r\n";
        return false;
    }

    // < iOS 10: Manifest.mbdb
    // >= iOS 10: Manifest.db
    if (!existsFile(combinePath(path, "Manifest.db")) && !existsFile(combinePath(path, "Manifest.mbdb")))
    {
        m_lastError += "Manifest.db/Manifest.mbdb not found\r\n";
        return false;
    }
    
    return true;
}

bool ManifestParser::isValidMobileSync(const std::string& path) const
{
    std::string backupPath = combinePath(path, "Backup");
    if (!existsFile(backupPath))
    {
        m_lastError += "Backup folder not found\r\n";
        return false;
    }

    return true;
}

bool ManifestParser::parse(const std::string& path, BackupManifest& manifest) const
{
    //Info.plist is a xml file
    if (!parseInfoPlist(path, manifest, m_incudingApps))
    {
		m_lastError += "Failed to parse xml: Info.plist\r\n";
        return false;
    }
    
    std::string fileName = combinePath(path, "Manifest.plist");
    std::vector<unsigned char> data;
    if (readFile(fileName, data))
    {
        plist_t node = NULL;
        plist_from_memory(reinterpret_cast<const char *>(&data[0]), static_cast<uint32_t>(data.size()), &node);
        if (NULL != node)
        {
            plist_t isEncryptedNode = plist_access_path(node, 1, "IsEncrypted");
            if (NULL != isEncryptedNode)
            {
                uint8_t val = 0;
                plist_get_bool_val(isEncryptedNode, &val);
                manifest.setEncrypted(val != 0);
            }
            
            if (manifest.getIOSVersion().empty())
            {
                plist_t iOSVersionNode = plist_access_path(node, 2, "Lockdown", "ProductVersion");
                manifest.setIOSVersion(getPlistStringValue(iOSVersionNode));
            }
            
            plist_free(node);
        }
    }
	else
	{
		m_lastError = "Failed to read Manifest.plist\r\n";
		return false;
	}

    return true;
}

bool ManifestParser::parseInfoPlist(const std::string& backupIdPath, BackupManifest& manifest, bool includingApps)
{
    std::string fileName = combinePath(backupIdPath, "Info.plist");
    std::string contents = readFile(fileName);
    plist_t node = NULL;
    plist_from_memory(contents.c_str(), static_cast<uint32_t>(contents.size()), &node);
    if (NULL == node)
    {
        return false;
    }
    
    manifest.setPath(backupIdPath);
    
    const char* ptr = NULL;
    uint64_t length = 0;
    std::string val;

    const char* ValueLastBackupDate = "Last Backup Date";
    const char* ValueDisplayName = "Display Name";
    const char* ValueDeviceName = "Device Name";
    const char* ValueITunesVersion = "iTunes Version";
    const char* ValueMacOSVersion = "macOS Version";
    const char* ValueProductVersion = "Product Version";
    const char* ValueInstalledApps = "Installed Applications";
    
    plist_t subNode = NULL;
    
    manifest.setDeviceName(getPlistStringValue(node, ValueDeviceName));
    manifest.setDisplayName(getPlistStringValue(node, ValueDisplayName));
    
    subNode = plist_dict_get_item(node, ValueLastBackupDate);
    if (NULL != subNode)
    {
        int32_t sec = 0, usec = 0;
        plist_get_date_val(subNode, &sec, &usec);
        manifest.setBackupTime(fromUnixTime(sec + 978278400));
    }
    
    manifest.setITunesVersion(getPlistStringValue(node, ValueITunesVersion));
    manifest.setIOSVersion(getPlistStringValue(node, ValueProductVersion));
    manifest.setMacOSVersion(getPlistStringValue(node, ValueMacOSVersion));
    
    if (includingApps)
    {
        subNode = plist_dict_get_item(node, ValueInstalledApps);
        if (NULL != subNode && PLIST_IS_ARRAY(subNode))
        {
            uint32_t arraySize = plist_array_get_size(subNode);
            plist_t itemNode = NULL;
            for (uint32_t idx = 0; idx < arraySize; ++idx)
            {
                itemNode = plist_array_get_item(subNode, idx);
                if (itemNode == NULL)
                {
                    continue;
                }
                std::string bundleId = getPlistStringValue(itemNode);
#ifndef NDEBUG
                if (bundleId == "hk.itools.apper")
                {
                    int aa = 0;
                }
#endif
                if (!bundleId.empty())
                {
                    plist_t appNode = plist_access_path(node, 2, "Applications", bundleId.c_str());
                    
                    if (NULL != appNode)
                    {
                        plist_t appSubNode = NULL;
                        
                        appSubNode = plist_dict_get_item(appNode, "iTunesMetadata");
                        if (NULL != appSubNode)
                        {
                            plist_type ptype = plist_get_node_type(appSubNode);
                            ptr = plist_get_data_ptr(appSubNode, &length);
                            if (ptr != NULL && length > 0)
                            {
                                // std::string metadata(ptr, length);
                                BackupManifest::AppInfo appInfo;
                                appInfo.bundleId = bundleId;
								if (!parseITunesMetadata(ptr, length, appInfo))
								{
#ifndef NDEBUG
									writeFile("Z:\\Documents\\WxExp\\dbg\\" + bundleId + ".plist", (const unsigned char *)ptr, length);
#endif
								}
                                manifest.addApp(appInfo);
                            }
                            
                        }
                        
                    }
                }
                
            }
        
        }
    }
    
    plist_free(node);

    return true;
}

bool ManifestParser::parseITunesMetadata(const char* metadata, size_t length, BackupManifest::AppInfo& appInfo)
{
    plist_t node = NULL;
    plist_from_memory(metadata, static_cast<uint32_t>(length), &node);
    if (NULL == node)
    {
        return false;
    }

    appInfo.name = getPlistStringValue(node, "itemName");
    appInfo.bundleShortVersion = getPlistStringValue(node, "bundleShortVersionString");
    appInfo.bundleVersion = getPlistStringValue(node, "bundleVersion");
    
    plist_free(node);
    
    return true;
}
