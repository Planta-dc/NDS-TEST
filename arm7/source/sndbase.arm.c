// arm7/source/sndbase.arm.c
// DSi MP3 Player - ARM7 audio output
// Based on SSEQPlayer by CaitSith2/Rocket Robz (stripped of SSEQ sequencer)
// MP3 streaming replaces the note synthesizer.
//
// The ARM7 owns channels 0 (left) and 1 (right).
// It plays the PCM ring buffer the ARM9 fills via libmad.
// When it has consumed a chunk it signals the ARM9 via FIFO_RETURN.

#include <nds.h>
#include <string.h>
#include <sndcommon.h>

// Must match arm9/source/sndbase.c
#define SOUND_BUFFER_SAMPLES 16384

volatile int mp3_status = STATUS_STOPPED;

// Pointers and params set when ARM9 sends SNDSYS_PLAYMP3
static volatile void *g_pcmL        = NULL;
static volatile void *g_pcmR        = NULL;
static volatile u32   g_sampleRate  = 44100;
static volatile u8    g_channels    = 2;

// ---------------------------------------------------------------------------
// Timer ISR - fires at ~18 kHz (ClockDivider_64, reload -2728)
// Each fire ≈ 1/18000 s.  TRANSFER_SAMPLES (1024) @ 44100 Hz ≈ 23 ms ≈ 414
// timer ticks.  We use a slightly conservative value so we ask for a refill
// before the buffer runs dry.
// ---------------------------------------------------------------------------
#define TIMER_TICKS_PER_CHUNK 380

static volatile u32 g_sample_countdown = TIMER_TICKS_PER_CHUNK;

static void sound_timer(void)
{
    if (mp3_status != STATUS_PLAYING) return;
    if (g_pcmL == NULL) return;

    if (g_sample_countdown > 0) {
        g_sample_countdown--;
        return;
    }
    g_sample_countdown = TIMER_TICKS_PER_CHUNK;

    refillMsg msg;
    msg.requestMore = 1;
    fifoSendDatamsg(FIFO_RETURN, sizeof(msg), (u8*)&msg);
}

// ---------------------------------------------------------------------------
// Start two looping PCM channels for stereo (or one for mono)
// ---------------------------------------------------------------------------
static void start_channels(void)
{
    // Use a timer value that gives roughly the right sample rate.
    // DS hardware: TIMER = -(CPU_FREQ / sample_rate)
    // CPU_FREQ for sound = 33513982 Hz
    u16 timer = (u16)(-(33513982 / g_sampleRate));

    // Left channel (ch 0)
    SCHANNEL_CR(0)           = 0;
    SCHANNEL_SOURCE(0)       = (u32)g_pcmL;
    SCHANNEL_TIMER(0)        = timer;
    SCHANNEL_REPEAT_POINT(0) = 0;
    // SCHANNEL_LENGTH is in 32-bit words. For 16-bit PCM: samples * 2 bytes / 4 = samples / 2
    SCHANNEL_LENGTH(0)       = (SOUND_BUFFER_SAMPLES * 2) / 4;
    SCHANNEL_CR(0)           = SCHANNEL_ENABLE | SOUND_REPEAT | SOUND_16BIT
                               | SOUND_VOL(127) | SOUND_PAN(0);

    if (g_channels == 2) {
        // Right channel (ch 1)
        SCHANNEL_CR(1)           = 0;
        SCHANNEL_SOURCE(1)       = (u32)g_pcmR;
        SCHANNEL_TIMER(1)        = timer;
        SCHANNEL_REPEAT_POINT(1) = 0;
        SCHANNEL_LENGTH(1)       = (SOUND_BUFFER_SAMPLES * 2) / 4;
        SCHANNEL_CR(1)           = SCHANNEL_ENABLE | SOUND_REPEAT | SOUND_16BIT
                                   | SOUND_VOL(127) | SOUND_PAN(127);
    }

    // Kick the timer countdown
    g_sample_countdown = TIMER_TICKS_PER_CHUNK;
    mp3_status = STATUS_PLAYING;
}

static void stop_channels(void)
{
    SCHANNEL_CR(0) = 0;
    SCHANNEL_CR(1) = 0;
    mp3_status = STATUS_STOPPED;
}

// ---------------------------------------------------------------------------
// FIFO message handler (ARM9 -> ARM7)
// ARM9 sends full mp3Msg for PLAY, but just a bare int for STOP/PAUSE.
// We read the first int to get the command, then read the rest if needed.
// ---------------------------------------------------------------------------
static void sndsysMsgHandler(int bytes, void *user_data)
{
    // Always read the full message bytes regardless of type
    u8 buf[sizeof(mp3Msg)];
    if (bytes > (int)sizeof(mp3Msg)) bytes = sizeof(mp3Msg);
    fifoGetDatamsg(FIFO_SNDSYS, bytes, buf);

    int cmd = *(int*)buf;

    switch (cmd)
    {
        case SNDSYS_PLAYMP3:
        {
            mp3Msg *msg = (mp3Msg*)buf;
            stop_channels();
            g_pcmL       = msg->pcmL;
            g_pcmR       = msg->pcmR;
            g_sampleRate = msg->sampleRate;
            g_channels   = msg->channels;
            start_channels();
            break;
        }
        case SNDSYS_STOPMP3:
            stop_channels();
            g_pcmL = NULL;
            g_pcmR = NULL;
            break;

        case SNDSYS_PAUSEMP3:
            if (mp3_status == STATUS_PLAYING) {
                SCHANNEL_CR(0) &= ~SCHANNEL_ENABLE;
                SCHANNEL_CR(1) &= ~SCHANNEL_ENABLE;
                mp3_status = STATUS_PAUSED;
            } else if (mp3_status == STATUS_PAUSED) {
                SCHANNEL_CR(0) |= SCHANNEL_ENABLE;
                SCHANNEL_CR(1) |= SCHANNEL_ENABLE;
                mp3_status = STATUS_PLAYING;
            }
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Public init - called from arm7/template.c
// ---------------------------------------------------------------------------
void InstallSoundSys(void)
{
    // Power on audio hardware
    powerOn(POWER_SOUND);
    writePowerManagement(PM_CONTROL_REG,
        (readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE) | PM_SOUND_AMP);
    REG_SOUNDCNT    = SOUND_ENABLE;
    REG_MASTER_VOLUME = 127;

    // Timer 1 at /64 divider, fires frequently enough to track buffer consumption
    timerStart(1, ClockDivider_64, -2728, sound_timer);

    // Install FIFO handler for messages from ARM9
    fifoSetDatamsgHandler(FIFO_SNDSYS, sndsysMsgHandler, 0);
}
