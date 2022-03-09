/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>

#include <stdlib.h>
#include <string.h>

#include "nds_getentropy.h"

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}


//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

	readUserSettings();
	ledBlink(0);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	touchInit();

	mmInstall(FIFO_MAXMOD);

	SetYtrigger(80);

	installWifiFIFO();
	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable(IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

	setPowerButtonCB(powerButtonCB);

	while (!exitflag) {
		if ( 0 == (REG_KEYINPUT & KEY_START))
			exitflag = true;

		if (fifoCheckAddress(FIFO_USER_01)) {
			struct pnl {
				void *buf;
				size_t len;
			} *pnl;
			pnl = fifoGetAddress(FIFO_USER_01);
			fifoSendValue32(FIFO_USER_01,
			    arm7_getentropy(pnl->buf, pnl->len));
		}
	}
	return 0;
}
