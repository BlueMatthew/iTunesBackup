//
//  Utils.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Utils.h"
#include <ctime>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <cstdio>
#include <chrono>
#ifdef _WIN32
#include <direct.h>
#include <atlstr.h>
#include <sys/utime.h>
#include <Shlwapi.h>
#ifndef NDEBUG
#include <cassert>
#endif
#else
#include <utime.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>
#include "FileSystem.h"

void replaceAll(std::string& input, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while((pos = input.find(search, pos)) != std::string::npos)
    {
        input.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

void replaceAll(std::string& input, const std::vector<std::pair<std::string, std::string>>& pairs)
{
    for (std::vector<std::pair<std::string, std::string>>::const_iterator it = pairs.cbegin(); it != pairs.cend(); ++it)
    {
        size_t pos = 0;
        while((pos = input.find(it->first, pos)) != std::string::npos)
        {
            input.replace(pos, it->first.length(), it->second);
            pos += it->second.length();
        }
    }
}

std::string replaceAll(const std::string& input, const std::string& search, const std::string& replace)
{
    std::string result = input;
    replaceAll(result, search, replace);
    return result;
}

std::string replaceAll(const std::string& input, const std::vector<std::pair<std::string, std::string>>& pairs)
{
    std::string result = input;
    replaceAll(result, pairs);

    return result;
}

bool endsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

bool endsWith(const std::string& str, std::string::value_type ch)
{
    return !str.empty() && str[str.size() - 1] == ch;
}

bool startsWith(const std::string& str, const std::string& prefix, int pos/* = 0*/)
{
    return str.size() >= prefix.size() && 0 == str.compare(pos, prefix.size(), prefix);
}

bool startsWith(const std::string& str, std::string::value_type ch)
{
    return !str.empty() && str[0] == ch;
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delimiter, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delimiter.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

std::string join(const std::vector<std::string>& elements, const char *const delimiter)
{
    return join(std::cbegin(elements), std::cend(elements), delimiter);
}

std::string join(std::vector<std::string>::const_iterator b, std::vector<std::string>::const_iterator e, const char *const delimiter)
{
    std::ostringstream os;
    if (b != e)
    {
        auto pe = prev(e);
        for (std::vector<std::string>::const_iterator it = b; it != pe; ++it)
        {
            os << *it;
            os << delimiter;
        }
        b = pe;
    }
    if (b != e)
    {
        os << *b;
    }

    return os.str();
}

std::string safeHTML(const std::string& s)
{
    static std::vector<std::pair<std::string, std::string>> replaces =
    { {"&", "&amp;"}, /*{" ", "&nbsp;"}, */{"<", "&lt;"}, {">", "&gt;"}, {"\r\n", "<br/>"}, {"\r", "<br/>"}, {"\n", "<br/>"} };
    return replaceAll(s, replaces);
}

void removeHtmlTags(std::string& html)
{
    std::string::size_type startpos = 0;
    while ((startpos = html.find("<", startpos)) != std::string::npos)
    {
        // auto startpos = html.find("<");
        auto endpos = html.find(">", startpos + 1);
        if (endpos == std::string::npos)
        {
            break;
        }
        html.erase(startpos, endpos - startpos + 1);
    }
}

std::string removeCdata(const std::string& str)
{
    if (startsWith(str, "<![CDATA[") && endsWith(str, "]]>")) return str.substr(9, str.size() - 12);
    return str;
}

std::string fromUnixTime(unsigned int unixtime, bool localTime/* = true*/)
{
    std::time_t ts = unixtime;
    std::tm* t1 = std::localtime(&ts);
    if (!localTime)
    {
        std::time_t local_secs = std::mktime(t1);

        struct tm *t2 = gmtime(&ts);
        std::time_t gmt_secs = mktime(t2);
        
        ts -= gmt_secs - local_secs;
        t1 = std::localtime(&ts);
    }
    
    char buf[30] = { 0 };
    std::strftime(buf, 30, "%Y-%m-%d %H:%M:%S", t1);

    // std::stringstream ss; // or if you're going to print, just input directly into the output stream
    // ss << std::put_time(t, "%Y-%m-%d %H:%M:%S");

    return std::string(buf);
}

uint32_t getUnixTimeStamp()
{
	time_t rawTime = 0;
	time(&rawTime);
	struct tm *localTm = localtime(&rawTime);
	return mktime(localTm);
}
/*
bool existsFile(const std::string &path)
{
#ifdef _WIN32
    struct stat buffer;
	CW2A pszA(CA2W(path.c_str(), CP_UTF8));
    return (stat ((LPCSTR)pszA, &buffer) == 0);
#else
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
#endif
}
*/

/*
int makePathImpl(const std::string::value_type *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0)
    {
        // Directory does not exist. EEXIST for race condition 
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        // errno = ENOTDIR;
        status = -1;
    }

    return status;
}

int makePath(const std::string& path, mode_t mode)
{
    std::vector<std::string::value_type> copypath;
    copypath.reserve(path.size() + 1);
    std::copy(path.begin(), path.end(), std::back_inserter(copypath));
    copypath.push_back('\0');
    std::replace(copypath.begin(), copypath.end(), '\\', '/');
    
    std::vector<std::string::value_type>::iterator itStart = copypath.begin();
    std::vector<std::string::value_type>::iterator it;
    
    int status = 0;
    while (status == 0 && (it = std::find(itStart, copypath.end(), '/')) != copypath.end())
    {
        if (it != copypath.begin())
        {
            // Neither root nor double slash in path
            *it = '\0';
            status = makePathImpl(&copypath[0], mode);
            *it = '/';
        }
        itStart = it + 1;
    }
    if (status == 0)
    {
        status = makePathImpl(&copypath[0], mode);
    }
    
    return status;
}
*/
/*
bool moveFile(const std::string& src, const std::string& dest, bool overwrite)
{
#ifdef _WIN32
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));
	if (overwrite)
	{
		::DeleteFile(pszDest);
	}
	BOOL bErrorFlag = ::MoveFile(pszSrc, pszDest);
	return (TRUE == bErrorFlag);
#else
	if (overwrite)
	{
		remove(dest.c_str());
	}
	return 0 == rename(src.c_str(), dest.c_str());
#endif
}

bool copyFile(const std::string& src, const std::string& dest)
{
#ifdef _WIN32
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));

	BOOL bErrorFlag = ::CopyFile(pszSrc, pszDest, FALSE);
	return (TRUE == bErrorFlag);
#else
	std::ifstream  ss(src, std::ios::binary);
	std::ofstream  ds(dest, std::ios::binary);

	ds << ss.rdbuf();

	return true;
#endif
}
*/

#ifdef _WIN32
std::string utf8ToLocalAnsi(const std::string& utf8Str)
{
	CW2A pszA(CA2W(utf8Str.c_str(), CP_UTF8));
	return std::string((LPCSTR)pszA);
}
#else
#endif

void updateFileTime(const std::string& path, time_t mtime)
{
#ifdef _WIN32
    CW2T pszT(CA2W(path.c_str(), CP_UTF8));
	struct _stat st;
	struct _utimbuf new_times;

    _tstat((LPCTSTR)pszT, &st);

	new_times.actime = st.st_atime; /* keep atime unchanged */
	new_times.modtime = mtime;

	_tutime((LPCTSTR)pszT, &new_times);
#else
	struct stat st;
	struct utimbuf new_times;

    stat(path.c_str(), &st);

	new_times.actime = st.st_atime; /* keep atime unchanged */
	new_times.modtime = mtime;

	utime(path.c_str(), &new_times);
#endif
}

/*
bool deleteFile(const std::string& fileName)
{
    return 0 == std::remove(fileName.c_str());
}
*/

int openSqlite3Database(const std::string& path, sqlite3 **ppDb, bool readOnly/* = true*/)
{
    std::string sep(1, DIR_SEP);
    std::string encodedPath;
#ifdef _WIN32
    TCHAR szDriver[_MAX_DRIVE] = { 0 };
    TCHAR szDir[_MAX_DIR] = { 0 };
    
    CW2T pszT(CA2W(normalizePath(path).c_str(), CP_UTF8));
    
    _tsplitpath(pszT, szDriver, szDir, NULL, NULL);
    size_t driveLen = _tcslen(szDriver);
    if (driveLen == 0)
    {
        // NO driver
        encodedPath = path;
    }
    else
    {
        CW2A pszU8(CT2W(&pszT[driveLen]), CP_UTF8);
        encodedPath = pszU8;
    }
#else
    encodedPath = normalizePath(path);
#endif

    std::vector<std::string> parts = split(encodedPath, sep);
    std::vector<std::string> encodedParts;
    encodedPath.reserve(parts.size() + 1);

	for (std::vector<std::string>::const_iterator it = parts.cbegin(); it != parts.cend(); ++it)
	{
		encodedParts.push_back(encodeUrl(*it));
	}

	// curl_easy_cleanup(curl);

	encodedPath = join(encodedParts, sep.c_str());

#ifdef _WIN32
    if (driveLen == 0)
    {
        if (_tcslen(szDir) > 0 && szDir[0] == DIR_SEP)
        {
            encodedPath = sep + encodedPath;
        }
    }
    else
    {
        CW2A pszU8(CT2W(szDriver), CP_UTF8);
        encodedPath = (LPCSTR)pszU8 + sep + encodedPath;
    }
#else
    if (startsWith(path, sep))
    {
        encodedPath = sep + encodedPath;
    }
#endif

#ifdef _WIN32
    std::string pathWithQuery = "file:///" + encodedPath;
#else
	std::string pathWithQuery = "file://" + encodedPath;
#endif
    // std::string pathWithQuery = "file:" + path;
    pathWithQuery += readOnly ? "?immutable=1&mode=ro" : "?mode=rw";
    
    // return sqlite3_open_v2(path.c_str(), ppDb, SQLITE_OPEN_READONLY, NULL);
    return sqlite3_open_v2(pathWithQuery.c_str(), ppDb, readOnly ? (SQLITE_OPEN_READONLY | SQLITE_OPEN_URI) : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI), NULL);
}


bool isBigEndian()
{
    short int number = 0x1;
    char *numPtr = (char*)&number;
    return (numPtr[0] != 1);
}

template<typename T>
T swapEndian(T u)
{
    union ET
    {
        T u;
        unsigned char u8[sizeof(T)];
    } src, dest;

    src.u = u;

    for (size_t i = 0; i < sizeof(T); ++i)
        dest.u8[i] = src.u8[sizeof(T) - i - 1];

    return dest.u;
}

int GetBigEndianInteger(const unsigned char* data, int startIndex/* = 0*/)
{
    if (isBigEndian())
    {
        return *((int *)(data + startIndex));
    }
    
#ifndef NDEBUG
    int aa =  (data[startIndex] << 24)
         | (data[startIndex + 1] << 16)
         | (data[startIndex + 2] << 8)
         | data[startIndex + 3];
    
    int bb = swapEndian(*((int *)(data + startIndex)));
    if (aa == bb)
    {
        aa = bb;
    }
#endif
    return swapEndian(*((int *)&data[startIndex]));
}

int16_t bigEndianToNative(int16_t n)
{
    return isBigEndian() ? n : swapEndian(n);
}

int32_t bigEndianToNative(int32_t n)
{
    return isBigEndian() ? n : swapEndian(n);
}

int64_t bigEndianToNative(int64_t n)
{
    return isBigEndian() ? n : swapEndian(n);
}

uint16_t bigEndianToNative(uint16_t n)
{
    return isBigEndian() ? n : swapEndian(n);
}

uint32_t bigEndianToNative(uint32_t n)
{
    return isBigEndian() ? n : swapEndian(n);
}

uint64_t bigEndianToNative(uint64_t n)
{
    return isBigEndian() ? n : swapEndian(n);
}


int GetLittleEndianInteger(const unsigned char* data, int startIndex/* = 0*/)
{
    return (data[startIndex + 3] << 24)
         | (data[startIndex + 2] << 16)
         | (data[startIndex + 1] << 8)
         | data[startIndex];
}

std::string encodeUrl(const std::string& url)
{
	std::string encodedUrl;
#ifdef _WIN32

    encodedUrl = url;
	CW2T szUrl(CA2W(url.c_str(), CP_UTF8));
	LPTSTR lpOutputBuffer = new TCHAR[1];
	DWORD dwSize = 1;
	HRESULT res = ::UrlEscape(szUrl, lpOutputBuffer, &dwSize, URL_ESCAPE_PERCENT | URL_ESCAPE_AS_UTF8);
	if (res == E_POINTER)
	{
		delete[] lpOutputBuffer;
		dwSize++;
		lpOutputBuffer = new TCHAR[dwSize];
		lpOutputBuffer[dwSize - 1] = 0;
		res = ::UrlEscape(szUrl, lpOutputBuffer, &dwSize, URL_ESCAPE_PERCENT | URL_ESCAPE_AS_UTF8);
		if (SUCCEEDED(res))
		{
			encodedUrl = CW2A(CT2W(lpOutputBuffer), CP_UTF8);
		}
	}
	if (lpOutputBuffer != NULL)
	{
		delete[] lpOutputBuffer;
		lpOutputBuffer = NULL;
	}
#else

#define IS_UNRESERVED(ch) (std::isalnum((char)ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~')
    
    const std::string::value_type* const hex = "0123456789ABCDEF";
    
    for (auto iter = url.begin(); iter != url.end(); ++iter)
    {
        // for utf8 encoded string, char ASCII can be greater than 127.
        int ch = static_cast<unsigned char>(*iter);
        // ch should be same under both utf8 and utf16.
        if (!IS_UNRESERVED(ch))
        {
            encodedUrl.push_back('%');
            encodedUrl.push_back(hex[(ch >> 4) & 0xF]);
            encodedUrl.push_back(hex[ch & 0xF]);
        }
        else
        {
            // ASCII don't need to be encoded, which should be same on both utf8 and utf16.
            encodedUrl.push_back(*iter);
        }
    }
    
#ifndef NDEBUG
    /*
    std::string encodedUrl2;
    CURL *curl = curl_easy_init();
    if(curl)
    {
        char *output = curl_easy_escape(curl, url.c_str(), static_cast<int>(url.size()));
        if(output)
        {
            encodedUrl2 = output;
            curl_free(output);
        }
        
        curl_easy_cleanup(curl);
    }
    
    assert(encodedUrl2 == encodedUrl);
     */
#endif

#endif
	return encodedUrl;
}

std::string getTimestampString(bool includingYMD/* = false*/, bool includingMs/* = false*/)
{
    using std::chrono::system_clock;
    auto currentTime = std::chrono::system_clock::now();
    char buffer[80];

    std::time_t tt;
    tt = system_clock::to_time_t ( currentTime );
    auto timeinfo = localtime (&tt);
    strftime (buffer, 80, includingYMD ? "%F %H:%M:%S" : "%H:%M:%S", timeinfo);
    if (includingMs)
    {
        auto transformed = currentTime.time_since_epoch().count() / 1000000;
        auto millis = transformed % 1000;
        sprintf(buffer, "%s.%03d", buffer, (int)millis);
    }

    return std::string(buffer);
}

bool isNumber(const std::string &s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}
