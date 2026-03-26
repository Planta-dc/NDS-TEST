// arm9/source/template.c
// DSi MP3 Player - ARM9 main
//
// Controls:
//   UP/DOWN       - scroll file list
//   LEFT/RIGHT    - page up/down (23 entries)
//   A             - play selected file
//   B (playing)   - stop and return to list
//   START         - pause / resume
//   L             - previous track (while playing)
//   R             - next track (while playing)
//   DOWN (playing)- toggle bottom backlight

#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sndcommon.h>
#include "frontend.h"
#include "sndbase.h"

// ---------------------------------------------------------------------------
// File list
// ---------------------------------------------------------------------------
#define MAX_FILES 5000
#define SCREEN_LINES 23

static char *g_files[MAX_FILES];
static u32   g_fileCount   = 0;
static u32   g_currentFile = 0;
static u32   g_lastFile    = 0xFFFFFFFF;

// Scrolling for long filenames
static u32 g_scrollPos    = 0;
static u32 g_scrollMax    = 0;
static u32 g_scrollCtr    = 0;
static u32 g_scrollDir    = 0; // 0=right, 1=left

static bool g_playing     = false;
static bool g_backlightOn = true;

// DSi RAM limit
static u32 g_maxRAM = 0x3C0000;

// argv forwarding (needed by frontend.c)
int    argc;
char **argv;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void toggleBacklight(void) {
    if (g_backlightOn)
        powerOff(PM_BACKLIGHT_BOTTOM);
    else
        powerOn(PM_BACKLIGHT_BOTTOM);
    g_backlightOn = !g_backlightOn;
}

// Returns 1 if filename ends with .mp3 (case-insensitive)
static int isMP3(const char *name) {
    int len = strlen(name);
    if (len < 4) return 0;
    return (tolower(name[len-1]) == '3' &&
            tolower(name[len-2]) == 'p' &&
            tolower(name[len-3]) == 'm' &&
            name[len-4] == '.');
}

// ---------------------------------------------------------------------------
// Recursive directory scan - fills g_files[]
// ---------------------------------------------------------------------------
static void scanDir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && g_fileCount < MAX_FILES) {
        // Skip . and ..
        if (ent->d_name[0] == '.' && (ent->d_name[1] == 0 ||
            (ent->d_name[1] == '.' && ent->d_name[2] == 0)))
            continue;

        // Build full path
        int pathlen = strlen(path);
        int namelen = strlen(ent->d_name);
        char *full = malloc(pathlen + 1 + namelen + 1);
        if (!full) continue;
        strcpy(full, path);
        if (path[pathlen-1] != '/') strcat(full, "/");
        strcat(full, ent->d_name);

        // Use stat() - d_type is unreliable with libfat
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            scanDir(full);
            free(full);
        } else if (isMP3(ent->d_name)) {
            g_files[g_fileCount++] = full;
        } else {
            free(full);
        }
    }
    closedir(dir);
}

static void ScanMP3s(void) {
    g_fileCount   = 0;
    g_currentFile = 0;
    printf("Scanning for MP3 files...\n");
    scanDir("/mp3");
    if (g_fileCount == 0) {
        // Also check root if /mp3 was empty
        scanDir("/");
    }
    printf("Found %lu files.\n", (unsigned long)g_fileCount);
}

// ---------------------------------------------------------------------------
// Display the file list on the bottom screen
// ---------------------------------------------------------------------------
static void ShowList(void) {
    consoleClear();

    if (g_fileCount == 0) {
        printf("No MP3 files found.\n");
        printf("Put .mp3 files in /mp3/\n");
        return;
    }

    for (u32 i = g_currentFile; i < g_currentFile + SCREEN_LINES; i++) {
        if (i >= g_fileCount) break;

        // Extract just the filename from the full path
        const char *fullpath = g_files[i];
        const char *name = strrchr(fullpath, '/');
        name = name ? name + 1 : fullpath;

        u32 temp = 0;
        if (i == g_currentFile) {
            temp = g_scrollPos;
            g_scrollMax = (strlen(name) > 0x1F) ? strlen(name) - 0x1F : 0;
            printf("*");
        } else {
            printf(" ");
        }

        if (strlen(name) <= 0x1F) {
            printf("%s\n", name);
        } else {
            char buf[0x21];
            memcpy(buf, name + temp, 0x1F);
            buf[0x1F] = 0;
            printf("%s", buf);
        }
    }
}

// ---------------------------------------------------------------------------
// Show now-playing screen
// ---------------------------------------------------------------------------
static void ShowPlaying(void) {
    consoleClear();
    if (g_currentFile >= g_fileCount) return;

    const char *fullpath = g_files[g_currentFile];
    const char *name = strrchr(fullpath, '/');
    name = name ? name + 1 : fullpath;

    printf("Now Playing:\n");

    // Scrolling name
    if (strlen(name) <= 30) {
        printf("%s\n", name);
    } else {
        char buf[32];
        u32 offset = g_scrollPos % (strlen(name) - 29 + 1);
        memcpy(buf, name + offset, 30);
        buf[30] = 0;
        printf("%s\n", buf);
    }

    printf("\n");
    printf("[B]  Stop\n");
    printf("[START] Pause\n");
    printf("[L/R] Prev/Next\n");
    printf("[DOWN] Backlight\n");
}

// ---------------------------------------------------------------------------
// Play the currently selected file
// ---------------------------------------------------------------------------
static void PlayCurrent(void) {
    if (g_fileCount == 0) return;
    StopMP3();
    consoleClear();
    printf("Loading...\n%s\n", g_files[g_currentFile]);
    PlayMP3(g_files[g_currentFile]);
    g_playing = true;
    g_scrollPos = 0;
    g_scrollCtr = 10;
    ShowPlaying();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int _argc, char **_argv) {
    argc = _argc;
    argv = _argv;

    // Turn off top backlight (text only on bottom)
    powerOff(PM_BACKLIGHT_TOP);
    consoleDemoInit();
    InstallSoundSys();

    if (isDSiMode()) {
        g_maxRAM = 0xF80000;
        printf("DSi mode - extended RAM\n");
    }

    if (!fatInitDefault()) {
        printf("FAT init FAILED\n");
        for(;;) swiWaitForVBlank();
    }

    // Check if launched with a specific file (from exploit / file manager)
    char launchPath[768] = {0};
    if (readFrontend(launchPath) && isMP3(launchPath)) {
        // Add it as the only entry and play immediately
        g_files[0]  = strdup(launchPath);
        g_fileCount = 1;
        g_currentFile = 0;
        PlayCurrent();
    } else {
        ScanMP3s();
        ShowList();
    }

    // Redraw flag - only redraw when state changes
    bool needRedraw = true;

    for (;;) {
        swiWaitForVBlank();

        // Decode more PCM if ARM7 asked for it (must happen every vblank)
        if (g_playing) {
            UpdateMP3();
        }

        // Auto-advance when track finishes
        if (g_playing && IsMP3Finished()) {
            if (g_currentFile + 1 < g_fileCount) {
                g_currentFile++;
                PlayCurrent();
            } else {
                StopMP3();
                g_playing = false;
                if (!g_backlightOn) toggleBacklight();
            }
            needRedraw = true;
        }

        // Reset scroll state when selection changes (before redraw)
        if (g_currentFile != g_lastFile) {
            g_lastFile   = g_currentFile;
            g_scrollPos  = 0;
            g_scrollCtr  = 10;
            g_scrollDir  = 0;
            g_scrollMax  = 0;
            needRedraw   = true;
        }

        // Update scrolling ticker
        if (g_scrollCtr == 0) {
            g_scrollCtr = 10;
            if (!g_scrollDir) {
                if (g_scrollPos < g_scrollMax) { g_scrollPos++; needRedraw = true; }
                else { g_scrollDir = 1; g_scrollCtr = 120; }
            } else {
                if (g_scrollPos > 0) { g_scrollPos--; needRedraw = true; }
                else { g_scrollDir = 0; g_scrollCtr = 120; }
            }
        } else {
            g_scrollCtr--;
        }

        // Redraw only when needed
        if (needRedraw) {
            needRedraw = false;
            if (g_playing)
                ShowPlaying();
            else
                ShowList();
        }

        scanKeys();
        u32 keys = keysDown();

        if (!g_playing) {
            // ---- Browse mode ----
            if (keys & KEY_UP) {
                if (g_currentFile > 0) g_currentFile--;
                else g_currentFile = (g_fileCount > 0) ? g_fileCount - 1 : 0;
                needRedraw = true;
            }
            if (keys & KEY_DOWN) {
                if (g_currentFile + 1 < g_fileCount) g_currentFile++;
                else g_currentFile = 0;
                needRedraw = true;
            }
            if (keys & KEY_LEFT) {
                g_currentFile = (g_currentFile >= SCREEN_LINES)
                                ? g_currentFile - SCREEN_LINES : 0;
                needRedraw = true;
            }
            if (keys & KEY_RIGHT) {
                g_currentFile += SCREEN_LINES;
                if (g_currentFile >= g_fileCount)
                    g_currentFile = g_fileCount > 0 ? g_fileCount - 1 : 0;
                needRedraw = true;
            }
            if (keys & KEY_A) {
                PlayCurrent();
                needRedraw = true;
            }
        } else {
            // ---- Playback mode ----
            if (keys & KEY_B) {
                StopMP3();
                g_playing = false;
                if (!g_backlightOn) toggleBacklight();
                needRedraw = true;
            }
            if (keys & KEY_START) {
                PauseMP3();
                needRedraw = true;
            }
            if (keys & KEY_L) {
                if (g_currentFile > 0) g_currentFile--;
                else g_currentFile = g_fileCount > 0 ? g_fileCount - 1 : 0;
                PlayCurrent();
                needRedraw = true;
            }
            if (keys & KEY_R) {
                g_currentFile = (g_currentFile + 1 < g_fileCount)
                                ? g_currentFile + 1 : 0;
                PlayCurrent();
                needRedraw = true;
            }
            if (keys & KEY_DOWN) {
                toggleBacklight();
            }
        }
    }
}
