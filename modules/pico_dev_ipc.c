/*********************************************************************
   PicoTCP. Copyright (c) 2012-2015 Altran Intelligent Systems. Some rights reserved.
   See LICENSE and COPYING for usage.

   Authors: Michiel Kustermans
 *********************************************************************/

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "pico_device.h"
#include "pico_dev_ipc.h"
#include "pico_stack.h"

struct pico_device_ipc {
    struct pico_device dev;
    int fd;
};

#define IPC_MTU 1500

static int pico_ipc_send(struct pico_device *dev, void *buf, int len)
{
    struct pico_device_ipc *ipc = (struct pico_device_ipc *) dev;
    return (int)write(ipc->fd, buf, (uint32_t)len);
}

static int pico_ipc_poll(struct pico_device *dev, int loop_score)
{
    struct pico_device_ipc *ipc = (struct pico_device_ipc *) dev;
    struct pollfd pfd;
    unsigned char buf[IPC_MTU];
    int len;
    pfd.fd = ipc->fd;
    pfd.events = POLLIN;
    do  {
        if (poll(&pfd, 1, 0) <= 0)
            return loop_score;

        len = (int)read(ipc->fd, buf, IPC_MTU);
        if (len > 0) {
            loop_score--;
            pico_stack_recv(dev, buf, (uint32_t)len);
        }
    } while(loop_score > 0);
    return 0;
}

/* Public interface: create/destroy. */

void pico_ipc_destroy(struct pico_device *dev)
{
    struct pico_device_ipc *ipc = (struct pico_device_ipc *) dev;
    if(ipc->fd > 0) {
        close(ipc->fd);
    }
}

static int ipc_connect(char *name)
{
    struct sockaddr_un addr;
    int ipc_fd;

    if((ipc_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        return(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);

    if(connect(ipc_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
        return(-1);
    }

    return ipc_fd;
}

struct pico_device *pico_ipc_create(char *name, uint8_t *mac)
{
    struct pico_device_ipc *ipc = PICO_ZALLOC(sizeof(struct pico_device_ipc));

    if (!ipc)
        return NULL;

    if( 0 != pico_device_init((struct pico_device *)ipc, name, mac)) {
        dbg("Ipc init failed.\n");
        pico_ipc_destroy((struct pico_device *)ipc);
        return NULL;
    }

    ipc->dev.overhead = 0;
    ipc->fd = ipc_connect(name);
    if (ipc->fd < 0) {
        dbg("Ipc creation failed.\n");
        pico_ipc_destroy((struct pico_device *)ipc);
        return NULL;
    }

    ipc->dev.send = pico_ipc_send;
    ipc->dev.poll = pico_ipc_poll;
    ipc->dev.destroy = pico_ipc_destroy;
    dbg("Device %s created.\n", ipc->dev.name);
    return (struct pico_device *)ipc;
}