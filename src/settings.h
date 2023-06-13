#ifndef SETTINGS_H
#define SETTINGS_H
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    char* key;
    char* value;
} KeyValuePair;

typedef struct {
    char path[MAXPATHLEN];
    char coverEnabled[2];
    char coverBlocks[2];
    char visualizationEnabled[2];
    char visualizationHeight[6];
} AppSettings;

extern AppSettings constructAppSettings(KeyValuePair* pairs, int count);

KeyValuePair* readKeyValuePairs(const char* file_path, int* count);

extern void freeKeyValuePairs(KeyValuePair* pairs, int count);

extern AppSettings constructAppSettings(KeyValuePair* pairs, int count);

// saves the path to your music folder
int saveSettingsDeprecated(char *path, const char* settingsFile);

// reads the settings file, which contains the path to your music folder
int getSettingsDeprecated(char *path, int len, const char* settingsFile);

#endif 