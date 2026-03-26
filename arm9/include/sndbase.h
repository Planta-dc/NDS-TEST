#pragma once

#include <sndcommon.h>

// Called once at startup to install FIFO handlers
void InstallSoundSys(void);

// Start playing an MP3 file. Blocks briefly to pre-fill the buffer.
void PlayMP3(const char *path);

// Stop playback immediately
void StopMP3(void);

// Toggle pause/resume
void PauseMP3(void);

// Call this every vblank while playing - decodes more frames into the ring buffer
void UpdateMP3(void);

// Returns 1 if currently playing or paused
int  IsPlayingMP3(void);

// Returns 1 if track has finished
int  IsMP3Finished(void);
