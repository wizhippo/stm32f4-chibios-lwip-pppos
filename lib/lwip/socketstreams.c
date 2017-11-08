#include <lib/lwip/socketstreams.h>
#include "ch.h"
#include "lwip/sockets.h"

static size_t socket_stream_write(void *ip, const uint8_t *bp, size_t n) {
	return lwip_write(((SocketStream*) ip)->fd, bp, n);
}

static size_t socket_stream_read(void *ip, uint8_t *bp, size_t n) {
	return lwip_read(((SocketStream*) ip)->fd, bp, n);
}

static msg_t socket_stream_put(void *ip, uint8_t b) {
	return lwip_write(((SocketStream*) ip)->fd, &b, sizeof(b));
}

static msg_t socket_stream_get(void *ip) {
	uint8_t b;

	if (lwip_read(((SocketStream*) ip)->fd, (uint8_t *) &b, sizeof(b)) < 0) {
		return -1;
	}

	return b;
}

static const struct SocketStreamVMT vmt = {
		socket_stream_write, socket_stream_read, socket_stream_put, socket_stream_get
};

void ssObjectInit(SocketStream *ssp, int fd) {
  ssp->vmt = &vmt;
  ssp->fd = fd;
}
