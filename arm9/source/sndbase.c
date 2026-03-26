// arm9/source/sndbase.c
// DSi MP3 Player - ARM9 libmad decode + FIFO to ARM7
//
// Architecture:
//   ARM9 decodes MP3 frames with libmad into a double-buffered 16-bit PCM
//   ring buffer.  The ARM7 plays the buffer on channels 0/1 in SOUND_REPEAT
//   mode.  When the ARM7 timer detects it has consumed a half-buffer it sends
//   a refillMsg back via FIFO_RETURN; UpdateMP3() decodes the next chunk.

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mad.h>
#include <sndcommon.h>
#include "sndbase.h"

// ---------------------------------------------------------------------------
// Buffer sizes (must match arm7/source/sndbase.arm.c expectations)
// ---------------------------------------------------------------------------
#define SOUND_BUFFER_SAMPLES   16384   // total ring buffer length (samples)
#define TRANSFER_SAMPLES        1024   // half-buffer: chunk sent to ARM7 per refill
#define INPUT_BUFFER_SIZE       8192   // compressed MP3 input buffer
#define FILE_READ_SIZE          8192   // how many bytes we read from SD at a time

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static struct mad_stream  g_stream;
static struct mad_frame   g_frame;
static struct mad_synth   g_synth;

// PCM ring buffers (16-bit) - must be in main RAM visible to ARM7
// We align to cache line to make DC_FlushRange efficient.
static s16 g_pcmL[SOUND_BUFFER_SAMPLES] __attribute__((aligned(32)));
static s16 g_pcmR[SOUND_BUFFER_SAMPLES] __attribute__((aligned(32)));

// Write cursor into the ring buffer
static int  g_buf_end      = 0; // next write position
static int  g_buf_pos      = 0; // next read position (tracks what ARM7 has played)
static int  g_buf_samples  = 0; // samples currently in buffer

// libmad compressed input
static unsigned char *g_input_buf  = NULL;
static unsigned char *g_guard_ptr  = NULL;
static char          *g_file_buf   = NULL;
static u32            g_file_buflen = 0;

static FILE *g_fp = NULL;
static u32   g_file_size = 0;

// Runtime flags
static int g_playing  = 0;
static int g_paused   = 0;
static int g_finished = 0;

// True when ARM7 has requested a refill
static volatile int g_refill_requested = 0;

// Sound params detected from first decoded frame
static u32 g_sample_rate = 44100;
static u8  g_channels    = 2;

// ---------------------------------------------------------------------------
// FIFO handlers
// ---------------------------------------------------------------------------
static void returnMsgHandler(int bytes, void *user_data)
{
    refillMsg msg;
    fifoGetDatamsg(FIFO_RETURN, bytes, (u8*)&msg);
    if (msg.requestMore)
        g_refill_requested = 1;
}

static void sndsysMsgHandler(int bytes, void *user_data)
{
    // ARM7 shouldn't send sndsysMsg back to us, but consume it to be safe
    u8 buf[64];
    fifoGetDatamsg(FIFO_SNDSYS, bytes, buf);
}

void InstallSoundSys(void)
{
    fifoSetDatamsgHandler(FIFO_SNDSYS, sndsysMsgHandler, 0);
    fifoSetDatamsgHandler(FIFO_RETURN, returnMsgHandler,  0);
}

// ---------------------------------------------------------------------------
// libmad fixed-point -> s16
// ---------------------------------------------------------------------------
static inline s16 fixed_to_s16(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));
    if (sample >  MAD_F_ONE - 1) sample =  MAD_F_ONE - 1;
    if (sample < -MAD_F_ONE)     sample = -MAD_F_ONE;
    return (s16)(sample >> (MAD_F_FRACBITS + 1 - 16));
}

// ---------------------------------------------------------------------------
// File-buffered read helpers (same double-buffered trick as LMP-ng)
// ---------------------------------------------------------------------------
static int mp3_read(void *ptr, u32 size)
{
    u32 read;

    // Top up the file buffer if low
    if (g_file_buflen < FILE_READ_SIZE)
        g_file_buflen += fread(g_file_buf + g_file_buflen, 1,
                               FILE_READ_SIZE - g_file_buflen, g_fp);

    if (g_file_buflen < size) {
        memcpy(ptr, g_file_buf, g_file_buflen);
        read = g_file_buflen;
        g_file_buflen = 0;
    } else {
        memcpy(ptr, g_file_buf, size);
        memmove(g_file_buf, g_file_buf + size, FILE_READ_SIZE - size);
        read = size;
        g_file_buflen -= size;
    }

    // Refill after consuming
    if (g_file_buflen < FILE_READ_SIZE)
        g_file_buflen += fread(g_file_buf + g_file_buflen, 1,
                               FILE_READ_SIZE - g_file_buflen, g_fp);
    return read;
}

static int mp3_eof(void)
{
    return (g_file_buflen == 0) && feof(g_fp);
}

// ---------------------------------------------------------------------------
// Decode one MAD frame and write samples into the ring buffer.
// Returns: 1 = ok/recoverable, 0 = finished, -1 = fatal error
// ---------------------------------------------------------------------------
static int decode_frame(void)
{
    if (g_finished) return 0;

    // Feed the stream
    if (g_stream.buffer == NULL || g_stream.error == MAD_ERROR_BUFLEN) {
        size_t remaining = 0;
        unsigned char *read_start;
        size_t read_size;

        if (g_stream.next_frame != NULL) {
            remaining  = g_stream.bufend - g_stream.next_frame;
            memmove(g_input_buf, g_stream.next_frame, remaining);
            read_start = g_input_buf + remaining;
            read_size  = INPUT_BUFFER_SIZE - remaining;
        } else {
            read_start = g_input_buf;
            read_size  = INPUT_BUFFER_SIZE;
        }

        mp3_read(read_start, read_size);

        if (mp3_eof()) {
            g_guard_ptr = read_start + read_size;
            memset(g_guard_ptr, 0, MAD_BUFFER_GUARD);
            read_size  += MAD_BUFFER_GUARD;
            g_finished  = 1;
        }

        mad_stream_buffer(&g_stream, g_input_buf, read_size + remaining);
        g_stream.error = 0;
    }

    if (mad_frame_decode(&g_frame, &g_stream)) {
        if (MAD_RECOVERABLE(g_stream.error)) return 1;
        if (g_stream.error == MAD_ERROR_BUFLEN)  return 1;
        return -1; // fatal
    }

    mad_synth_frame(&g_synth, &g_frame);

    // Grab audio params from first good frame
    g_sample_rate = g_frame.header.samplerate;
    g_channels    = MAD_NCHANNELS(&g_frame.header);

    // Write PCM into ring buffer
    int n = g_synth.pcm.length;
    for (int i = 0; i < n; i++) {
        g_pcmL[g_buf_end] = fixed_to_s16(g_synth.pcm.samples[0][i]);
        g_pcmR[g_buf_end] = (g_channels == 2)
                            ? fixed_to_s16(g_synth.pcm.samples[1][i])
                            : g_pcmL[g_buf_end];
        g_buf_end = (g_buf_end + 1) % SOUND_BUFFER_SAMPLES;
        g_buf_samples++;
    }

    return (g_finished && g_stream.error == MAD_ERROR_BUFLEN) ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Fill the buffer up to SOUND_BUFFER_SAMPLES or until EOF
// ---------------------------------------------------------------------------
static void fill_buffer(void)
{
    int status;
    while (g_buf_samples < SOUND_BUFFER_SAMPLES) {
        status = decode_frame();
        if (status <= 0) break;
    }
    // Flush to main RAM so ARM7 can DMA it
    DC_FlushRange(g_pcmL, SOUND_BUFFER_SAMPLES * 2);
    DC_FlushRange(g_pcmR, SOUND_BUFFER_SAMPLES * 2);
}

// ---------------------------------------------------------------------------
// Send start message to ARM7
// ---------------------------------------------------------------------------
static void send_play_msg(void)
{
    mp3Msg msg;
    msg.msg           = SNDSYS_PLAYMP3;
    msg.pcmL          = (void*)g_pcmL;
    msg.pcmR          = (void*)g_pcmR;
    msg.sampleRate    = g_sample_rate;
    msg.channels      = g_channels;
    msg.bufferSamples = SOUND_BUFFER_SAMPLES;
    fifoSendDatamsg(FIFO_SNDSYS, sizeof(msg), (u8*)&msg);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void PlayMP3(const char *path)
{
    StopMP3();

    // Alloc scratch buffers
    g_input_buf = (unsigned char*)malloc(INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD);
    g_file_buf  = (char*)malloc(FILE_READ_SIZE);
    if (!g_input_buf || !g_file_buf) return;

    g_fp = fopen(path, "rb");
    if (!g_fp) { free(g_input_buf); free(g_file_buf); return; }

    // Get file size
    fseek(g_fp, 0, SEEK_END);
    g_file_size = ftell(g_fp);
    fseek(g_fp, 0, SEEK_SET);

    // Init libmad
    mad_stream_init(&g_stream);
    mad_frame_init(&g_frame);
    mad_synth_init(&g_synth);

    g_buf_end      = 0;
    g_buf_pos      = 0;
    g_buf_samples  = 0;
    g_file_buflen  = 0;
    g_guard_ptr    = NULL;
    g_finished     = 0;
    g_refill_requested = 0;

    // Pre-fill: decode enough frames to get sample rate and fill the buffer
    // We need at least one frame decoded before we know sample_rate/channels
    int status;
    while (g_buf_samples == 0) {
        status = decode_frame();
        if (status <= 0) break;
    }
    // Now fill the rest
    fill_buffer();

    g_playing = 1;
    g_paused  = 0;

    send_play_msg();
}

void StopMP3(void)
{
    if (g_playing || g_paused) {
        // Send a minimal stop command - just the msg int, ARM7 only reads msg field
        int stop_cmd = SNDSYS_STOPMP3;
        fifoSendDatamsg(FIFO_SNDSYS, sizeof(stop_cmd), (u8*)&stop_cmd);
        swiWaitForVBlank();
    }

    if (g_fp) {
        mad_synth_finish(&g_synth);
        mad_frame_finish(&g_frame);
        mad_stream_finish(&g_stream);
        fclose(g_fp);
        g_fp = NULL;
    }
    if (g_input_buf) { free(g_input_buf); g_input_buf = NULL; }
    if (g_file_buf)  { free(g_file_buf);  g_file_buf  = NULL; }

    g_playing  = 0;
    g_paused   = 0;
    g_finished = 0;
    g_refill_requested = 0;
}

void PauseMP3(void)
{
    if (!g_playing && !g_paused) return;
    int pause_cmd = SNDSYS_PAUSEMP3;
    fifoSendDatamsg(FIFO_SNDSYS, sizeof(pause_cmd), (u8*)&pause_cmd);
    g_paused  = !g_paused;
    g_playing = !g_playing;
}

// Called every vblank from the main loop
void UpdateMP3(void)
{
    if (!g_playing) return;
    if (!g_refill_requested) return;
    g_refill_requested = 0;

    // Advance read cursor by one transfer chunk
    g_buf_pos     = (g_buf_pos + TRANSFER_SAMPLES) % SOUND_BUFFER_SAMPLES;
    g_buf_samples -= TRANSFER_SAMPLES;
    if (g_buf_samples < 0) g_buf_samples = 0;

    if (g_finished && g_buf_samples == 0) {
        g_playing = 0;
        return;
    }

    // Decode more frames to refill what was consumed
    fill_buffer();
}

int IsPlayingMP3(void)  { return g_playing || g_paused; }
int IsMP3Finished(void) { return g_finished && !g_playing; }
