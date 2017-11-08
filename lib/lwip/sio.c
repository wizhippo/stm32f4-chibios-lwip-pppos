#include <lib/lwip/sio.h>
#include "ch.h"
#include "hal.h"
#include "lwip/sio.h"

static int sio_abort;

/**
 * Reads from the serial device.
 *
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received - may be 0 if aborted by sio_read_abort
 *
 * @note This function will block until data can be received. The blocking
 * can be cancelled by calling sio_read_abort().
 */
u32_t sio_read(sio_fd_t fd, u8_t *data, u32_t len) {
	size_t n = 0;
	sio_abort = 0;
	while ((sio_abort == 0) && (n < 1)){
		n = ((SerialDriver*)fd)->vmt->readt(fd, data, len, TIME_IMMEDIATE);
	}
	return n;
}

/**
 * Writes to the serial device.
 *
 * @param fd serial device handle
 * @param data pointer to data to send
 * @param len length (in bytes) of data to send
 * @return number of bytes actually sent
 *
 * @note This function will block until all data can be sent.
 */
u32_t sio_write(sio_fd_t fd, u8_t *data, u32_t len) {
	return ((SerialDriver*)fd)->vmt->write(fd, data, len);
}

/**
 * Aborts a blocking sio_read() call.
 *
 * @param fd serial device handle
 */
void  sio_read_abort(sio_fd_t fd) {
	(void) fd;
	sio_abort = 1;
}
