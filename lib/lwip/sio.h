#ifndef SIO_H_
#define SIO_H_

#include "ch.h"
#include "hal_streams.h"

#ifndef __sio_fd_t_defined
#define __sio_fd_t_defined
typedef BaseSequentialStream* sio_fd_t;
#endif

#endif /* SIO_H_ */
