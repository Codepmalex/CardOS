#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

struct FileEntry {
    String name;
    String path;
    bool isDir = false;
    uint32_t size = 0;
};

struct SearchMatch {
    enum Kind : uint8_t {
        MATCH_STOCK_APP = 0,
        MATCH_SD_BIN,
        MATCH_FILE,
        MATCH_DIR
    };
    Kind kind = MATCH_FILE;
    String title;
    String subtitle;
    String path;
    uint8_t stockAppId = 0;
};

bool initStorage();
void refreshSdStats();
fs::FS& activeFS();
const char* activeFSName();

bool ensureDir(const char* path);
std::vector<FileEntry> listDir(const char* dirPath);
std::vector<FileEntry> listSdBinApps();

String readTextFile(const char* path, size_t maxBytes = 16384);
bool writeTextFile(const char* path, const String& content);
bool deletePathRecursive(const char* path);

std::vector<SearchMatch> searchFilesystem(const String& query, int maxResults = 16);
String formatBytes(uint32_t bytes);

