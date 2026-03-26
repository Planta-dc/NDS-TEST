// arm7/source/template.c
// Standard libnds ARM7 core - unchanged from SSEQPlayer
#include <nds.h>
#include <sndcommon.h>

void VblankHandler(void) {
    inputGetAndSend();
}

volatile bool exitflag = false;

void powerButtonCB() {
    exitflag = true;
}

int main(void) {
    readUserSettings();
    ledBlink(LED_ALWAYS_ON);

    irqInit();
    fifoInit();

    installSoundFIFO();
    installSystemFIFO();

    setPowerButtonCB(powerButtonCB);
    initClockIRQTimer(3);

    InstallSoundSys();

    irqSet(IRQ_VBLANK, VblankHandler);
    irqEnable(IRQ_VBLANK);

    while (!exitflag) {
        if (0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R)))
            exitflag = true;
        swiWaitForVBlank();
    }
    return 0;
}
