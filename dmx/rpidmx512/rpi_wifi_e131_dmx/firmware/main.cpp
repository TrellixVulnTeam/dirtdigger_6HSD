/**
 * @file main.c
 *
 */
/* Copyright (C) 2016-2018 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>
#include <uuid/uuid.h>

#include "hardwarebaremetal.h"
#include "networkbaremetal.h"
#include "ledblinkbaremetal.h"

#include "console.h"
#if defined (HAVE_I2C)
 #include "display.h"
#endif

#include "wifi.h"

#include "e131bridge.h"
#include "e131uuid.h"
#include "e131params.h"

// DMX output
#include "dmxparams.h"
#include "dmxsend.h"
#ifndef H3
 // Monitor Output
 #include "dmxmonitor.h"
#endif
#if defined (HAVE_SPI)
 // WS28xx output
 #include "ws28xxstripeparams.h"
 #include "ws28xxstripedmx.h"
#endif

#include "util.h"

#include "software_version.h"

extern "C" {

void notmain(void) {
	HardwareBaremetal hw;
	NetworkBaremetal nw;
	LedBlinkBaremetal lb;
	E131Params e131params;
	E131Uuid e131uuid;
	uuid_t uuid;
	char uuid_str[UUID_STRING_LENGTH + 1];
	uint8_t nHwTextLength;

	if (e131params.Load()) {
		e131params.Dump();
	}

	const TOutputType tOutputType = e131params.GetOutputType();

#if defined (HAVE_I2C)
	Display display(0,8);
	const bool IsOledConnected = display.isDetected();
#endif

	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hw.GetBoardName(nHwTextLength), __DATE__, __TIME__);

	console_puts("WiFi sACN E1.31 ");
	console_set_fg_color(tOutputType == OUTPUT_TYPE_DMX ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("DMX Output");
	console_set_fg_color(CONSOLE_WHITE);
#ifndef H3
	console_puts(" / ");
	console_set_fg_color(tOutputType == OUTPUT_TYPE_MONITOR ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("Real-time DMX Monitor");
	console_set_fg_color(CONSOLE_WHITE);
#endif
#if defined (HAVE_SPI)
	console_puts(" / ");
	console_set_fg_color(tOutputType == OUTPUT_TYPE_SPI ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("Pixel controller {1 Universe}");
	console_set_fg_color(CONSOLE_WHITE);
#endif
#ifdef H3
	console_putc('\n');
#endif

	hw.SetLed(HARDWARE_LED_ON);

	console_set_top_row(3);

	console_status(CONSOLE_YELLOW, "Network init ...");
#if defined (HAVE_I2C)
	DISPLAY_CONNECTED(IsOledConnected, display.TextStatus("Network init ..."));
#endif

	nw.Init();

	if (e131params.isHaveCustomCid()) {
		memcpy(uuid_str, e131params.GetCidString(), UUID_STRING_LENGTH);
		uuid_str[UUID_STRING_LENGTH] = '\0';
		uuid_parse((const char *)uuid_str, uuid);
	} else {
		e131uuid.GetHardwareUuid(uuid);
		uuid_unparse(uuid, uuid_str);
	}

	E131Bridge bridge;
	DMXSend dmx;
#if defined (HAVE_SPI)
	SPISend spi;
#endif
#ifndef H3
	DMXMonitor monitor;
#endif

	bridge.SetCid(uuid);
	bridge.SetUniverse(e131params.GetUniverse());
	bridge.SetMergeMode(e131params.GetMergeMode());

	if (tOutputType == OUTPUT_TYPE_DMX) {
		DMXParams dmxparams;
		if (dmxparams.Load()) {
			dmxparams.Dump();
		}
		dmxparams.Set(&dmx);
		bridge.SetOutput(&dmx);
	}
#if defined (HAVE_SPI)
	else if (tOutputType == OUTPUT_TYPE_SPI) {
		WS28XXStripeParams deviceparms;
		if (deviceparms.Load()) {
			deviceparms.Dump();
		}
		deviceparms.Set(&spi);
		bridge.SetOutput(&spi);
	}
#endif
#ifndef H3
	else if (tOutputType == OUTPUT_TYPE_MONITOR) {
		bridge.SetOutput(&monitor);
		monitor.Cls();
		console_set_top_row(20);
	}
#endif

	bridge.Print();

	if (tOutputType == OUTPUT_TYPE_DMX) {
		dmx.Print();
	}
#if defined (HAVE_SPI)
	else if (tOutputType == OUTPUT_TYPE_SPI) {
		spi.Print();
	}
#endif

#if defined (HAVE_I2C)
	if (IsOledConnected) {
		display.Write(1, "WiFi sACN E1.31 ");

		switch (tOutputType) {
		case OUTPUT_TYPE_DMX:
			display.PutString("DMX");
			break;
		case OUTPUT_TYPE_MONITOR:
			display.PutString("Mon");
			break;
		case OUTPUT_TYPE_SPI:
			display.PutString("Pixel");
			break;
		default:
			display.PutString("-E-");
			break;
		}

		if (wifi_get_opmode() == WIFI_STA) {
			(void) display.Printf(2, "S: %s", wifi_get_ssid());
		} else {
			(void) display.Printf(2, "AP (%s)\n", wifi_ap_is_open() ? "Open" : "WPA_WPA2_PSK");
		}

		(void) display.Printf(3, "CID: ");
		(void) display.PutString(uuid_str);
		(void) display.Printf(5, "U: %d M: %s", bridge.GetUniverse(), bridge.GetMergeMode() == E131_MERGE_HTP ? "HTP" : "LTP");
		(void) display.Printf(6, "M: " IPSTR "", IP2STR(bridge.GetMulticastIp()));
		(void) display.Printf(7, "U: " IPSTR "", IP2STR(Network::Get()->GetIp()));
	}
#endif

	console_status(CONSOLE_YELLOW, "Starting the Bridge ...");
#if defined (HAVE_I2C)
	DISPLAY_CONNECTED(IsOledConnected, display.TextStatus("Starting the Bridge ..."));
#endif

	bridge.Start();

	console_status(CONSOLE_GREEN, "Bridge is running");
#if defined (HAVE_I2C)
	DISPLAY_CONNECTED(IsOledConnected, display.TextStatus("Bridge is running"));
#endif

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		(void) bridge.Run();
		lb.Run();
	}
}

}
