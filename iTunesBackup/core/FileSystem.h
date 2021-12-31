//
//  FileSystem.h
//  WechatExporter
//
//  Created by Matthew on 2021/1/21.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef FileSystem_h
#define FileSystem_h

#include <string>
#include <vector>

#ifdef _WIN32
#define DIR_SEP '\\'
#define DIR_SEP_STR "\\"
#define ALT_DIR_SEP '/'
#else
#define DIR_SEP '/'
#define DIR_SEP_STR "/"
#define ALT_DIR_SEP '\\'
#endif

size_t getFileSize(const std::string& path);
bool existsDirectory(const std::string& path);
bool makeDirectory(const std::string& path);
bool deleteFile(const std::string& path);
bool deleteDirectory(const std::string& path);
bool existsFile(const std::string& path);
bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories);
bool copyFile(const std::string& src, const std::string& dest, bool overwrite = true);
bool moveFile(const std::string& src, const std::string& dest, bool overwrite = true);
// ref: https://blackbeltreview.wordpress.com/2015/01/27/illegal-filename-characters-on-windows-vs-mac-os/
bool isValidFileName(const std::string& fileName);
std::string removeInvalidCharsForFileName(const std::string& fileName);

std::string readFile(const std::string& path);
bool readFile(const std::string& path, std::vector<unsigned char>& data);
bool writeFile(const std::string& path, const std::vector<unsigned char>& data);
bool writeFile(const std::string& path, const std::string& data);
bool writeFile(const std::string& path, const unsigned char* data, size_t dataLength);
bool appendFile(const std::string& path, const std::string& data);
bool appendFile(const std::string& path, const unsigned char* data, size_t dataLength);



std::string combinePath(const std::string& p1, const std::string& p2);
std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3);
std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3, const std::string& p4);

std::string normalizePath(const std::string& path);
void normalizePath(std::string& path);


#endif /* FileSystem_h */
