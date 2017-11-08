/*
    ChibiOS - Copyright (C) 2006-2014 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "ch_test.h"

#include "chprintf.h"
#include "shell.h"
#include "ch_test.h"

#include "lwip/sockets.h"
#include "lwip/tcpip.h"
#include "ppp/ppp.h"
#include "lib/lwip/socketstreams.h"


/*
 * Shell related
 */
#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(1024)

static const ShellCommand commands[] = {
  {NULL, NULL}
};

static const ShellConfig shell_cfg = {
  (BaseSequentialStream *)&SD2,
  commands
};

static thread_t *shelltp = NULL;

/*
 * UDP Echo server thread
 */
static THD_WORKING_AREA(waEchoServerThread, 512 + 1024);
static THD_FUNCTION(EchoServerThread, arg) {
	(void) arg;

	int sock;
	struct sockaddr_in sa;
	char buffer[1024];
	int recsize;
	socklen_t fromlen;

	chRegSetThreadName("EchoServerThread");

	sock = lwip_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		return;
	}

	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(8000);
	fromlen = sizeof(sa);

	if (lwip_bind(sock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
		lwip_close(sock);
		return;
	}

	while (!chThdShouldTerminateX()) {
		recsize = lwip_recvfrom(sock, buffer, sizeof(buffer), 0,
				(struct sockaddr *) &sa, &fromlen);
		if (recsize < 0) {
			break;
		}
		lwip_sendto(sock, (void *) buffer, recsize, 0, (struct sockaddr *) &sa,
				fromlen);
	}

	lwip_close(sock);
}


/*
 * TCP Shell server thread
 */
static THD_WORKING_AREA(waShellServerThread, 512);
static THD_FUNCTION(ShellServerThread, arg) {
	(void) arg;

	int sockfd, newsockfd;
	socklen_t cli_addr_len;
	struct sockaddr_in serv_addr, cli_addr;

	SocketStream sbp;

	chRegSetThreadName("ShellServerThread");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = PP_HTONS(25);

	sockfd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0)
		return;

	if (lwip_bind(sockfd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		lwip_close(sockfd);
		return;
	}

	if (lwip_listen(sockfd, 1) < 0) {
		lwip_close(sockfd);
		return;
	}

	cli_addr_len = sizeof(cli_addr);

	while (!chThdShouldTerminateX()) {
		newsockfd = lwip_accept(sockfd, (struct sockaddr *) &cli_addr,
				&cli_addr_len);
		if (newsockfd < 0) {
			break;
		}

		ssObjectInit(&sbp, newsockfd);
		shelltp = chThdCreateFromHeap(NULL, SHELL_WA_SIZE, "shell", NORMALPRIO + 1, shellThread, (void *)&shell_cfg);
		chThdWait(shelltp);

		lwip_close(newsockfd);
	}

	if (lwip_shutdown(sockfd, SHUT_RDWR) < 0) {
		// oops
	}
	lwip_close(sockfd);
}


/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
  (void)arg;

  chRegSetThreadName("Thread1");

  while (TRUE) {
    palSetPad(GPIOD, GPIOD_LED3);       /* Orange.  */
    chThdSleepMilliseconds(500);
    palClearPad(GPIOD, GPIOD_LED3);     /* Orange.  */
    chThdSleepMilliseconds(500);
  }
}


/*
 * Handle link status events
 */
static void ppp_linkstatus_callback(void *ctx, int errCode, void *arg) {
	(void) arg;

	volatile int *connected = (int*)ctx;

	if (errCode == PPPERR_NONE) {
		*connected = 1;
	} else {
		*connected = 0;
	}
}


/*
 * Application entry point.
 */
int main(void) {
	thread_t *echoServerThread = 0;
	thread_t *shellServerThread = 0;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Shell manager initialization.
   */
  shellInit();

  /*
   * Activates the serial driver 2 using the driver default configuration.
   * PA2(TX) and PA3(RX) are routed to USART2.
   */
  sdStart(&SD2, NULL);
  palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));

  /*
   * Creates the example thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO + 1, Thread1, NULL);

  /*
   * lwip ppp
   */
	tcpip_init(NULL, NULL);
	pppInit();

	chThdSetPriority(PPP_THREAD_PRIO + 1);

  /*
   * Keep ppp connection up.
   */
	while (TRUE) {
		volatile int connected = 0;

		int pd = pppOverSerialOpen(&SD2, ppp_linkstatus_callback,
				(int*) &connected);

		if (pd < 0) {
			chThdSleep(MS2ST(100));
			continue;
		}

		// Wait for initial connection
		int timeout = 0;
		while (connected < 1) {
			chThdSleep(MS2ST(300));
			if(timeout++ > 5) {  // If we waited too long restart connection
				pppClose(pd);
				continue;
			}
		}

		// Make sure connection is stable
		while (connected < 5) {
			chThdSleep(MS2ST(100));
			if (connected == 0) { // reset by pppThread while waiting for stable connection
				pppClose(pd);
				continue;
			}
			connected++;
		}

		// Run server threads
		echoServerThread = chThdCreateStatic(waEchoServerThread,
				sizeof(waEchoServerThread), NORMALPRIO + 1, EchoServerThread, NULL);
		shellServerThread = chThdCreateStatic(waShellServerThread,
				sizeof(waShellServerThread), NORMALPRIO + 1, ShellServerThread,
				NULL);

		while (connected > 0) {
			chThdSleep(MS2ST(200));
		}

		// Tear down threads
		chThdTerminate(echoServerThread);
		chThdWait(echoServerThread);
		chThdTerminate(shellServerThread);
		chThdWait(shellServerThread);
	}

}
