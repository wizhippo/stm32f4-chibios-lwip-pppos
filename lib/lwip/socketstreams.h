#ifndef SOCKETSTREAMS_H_
#define SOCKETSTREAMS_H_

#include "ch.h"
#include "hal_streams.h"

#define _socket_stream_data     \
  _base_sequential_stream_data  \
  int fd

struct SocketStreamVMT {
	_base_sequential_stream_methods
};

typedef struct {
  const struct SocketStreamVMT *vmt;
  _socket_stream_data;
} SocketStream;

#ifdef __cplusplus
extern "C" {
#endif
	void ssObjectInit(SocketStream *ssp, int fd);
#ifdef __cplusplus
}
#endif

#endif /* SOCKETSTREAMS_H_ */
