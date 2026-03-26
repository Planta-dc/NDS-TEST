#pragma once

#define FIFO_SNDSYS FIFO_USER_01
#define FIFO_RETURN FIFO_USER_02

// Message types ARM9 -> ARM7
enum {
    SNDSYS_PLAYMP3 = 0,
    SNDSYS_STOPMP3,
    SNDSYS_PAUSEMP3,
};

// Status codes ARM7 -> ARM9
enum {
    STATUS_PLAYING,
    STATUS_STOPPED,
    STATUS_PAUSED,
};

// Sent ARM9->ARM7 to start playback
// Points ARM7 at the double-buffered PCM data in shared RAM
typedef struct {
    int      msg;
    void    *pcmL;          // left channel buffer (16-bit PCM)
    void    *pcmR;          // right channel buffer (16-bit PCM)
    u32      sampleRate;    // e.g. 44100
    u8       channels;      // 1 = mono, 2 = stereo
    u32      bufferSamples; // samples per half-buffer (transfer chunk)
} mp3Msg;

// Sent ARM7->ARM9 when it finishes consuming a chunk and needs more
typedef struct {
    u8 requestMore; // 1 = please decode more PCM
} refillMsg;

#ifdef ARM7

void InstallSoundSys(void);

volatile extern int mp3_status;

#endif

#ifdef ARM9

void InstallSoundSys(void);

#endif
