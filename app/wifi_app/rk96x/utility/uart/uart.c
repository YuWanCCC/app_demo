#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef ANDROID
#include <syslog.h>
#endif
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/timerfd.h>

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <utils/Log.h>
#include <assert.h>

#include "uart.h"
#include "slog.h"

#ifdef NEED_PPOLL
#include "ppoll.h"
#endif

/* #define SCHED_ENABLE */

#ifdef SCHED_ENABLE
#include <sched.h>
#endif

int uart_speed(int s)
{
    switch (s)
    {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
#ifdef B2500000
    case 2500000:
        return B2500000;
#endif
#ifdef B3000000
    case 3000000:
        return B3000000;
#endif
#ifdef B3500000
    case 3500000:
        return B3500000;
#endif
#ifdef B4000000
    case 4000000:
        return B4000000;
#endif
    default:
        return B57600;
    }
}

//set uart in and out Baud rate
int set_speed(int fd, struct termios *ti, int speed)
{
    if (cfsetospeed(ti, uart_speed(speed)) < 0)
        return -errno;

    if (cfsetispeed(ti, uart_speed(speed)) < 0)
        return -errno;

    if (tcsetattr(fd, TCSANOW, ti) < 0)
        return -errno;

    if (tcflush(fd, TCIOFLUSH) < 0)
        return -errno;

    return 0;
}

/* Initialize UART driver */
int uart_init(const char *dev, struct uart_t *u, int send_break, int raw)
{
    struct termios ti;
    int fd;
    unsigned long flags = 0;

    if (raw)
        flags |= 1 << HCI_UART_RAW_DEVICE;

    fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        pr_err("Can't open serial port - %s\n", strerror(errno));
        return -1;
    }

    tcflush(fd, TCIOFLUSH);  //flush in or out buffer

    //get uart attributation and save to ti
    if (tcgetattr(fd, &ti) < 0)
    {
        pr_err("Can't get port settings - %s\n", strerror(errno));
        return -1;
    }

    //set terminal to raw mode. raw data transform in UART without system IO
    cfmakeraw(&ti);

    //set uart control flags. CLOCAL: ignore modem control signal. CRTSCTS: hardware flow control
    ti.c_cflag |= CLOCAL;
    if (u->flags & FLOW_CTL)
        ti.c_cflag |= CRTSCTS;
    else
        ti.c_cflag &= ~CRTSCTS;

    //set uart configuration to opend fd. TCSANOW:take effect immediately
    if (tcsetattr(fd, TCSANOW, &ti) < 0)
    {
        pr_err("Can't set port settings - %s\n", strerror(errno));
        return -1;
    }

    /* Set initial baudrate */
    if (set_speed(fd, &ti, u->init_speed) < 0)
    {
        pr_err("Can't set initial baud rate - %s\n", strerror(errno));
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    if (send_break)
    {
        tcsendbreak(fd, 0); //send uart INT signal
        usleep(500000);
    }

    if (u->init && u->init(fd, u, &ti) < 0)
        return -1;

    tcflush(fd, TCIOFLUSH);

    /* Set actual baudrate
     * There is no need to change baudrate after uart init
     * */
    /* if (set_speed(fd, &ti, u->speed) < 0) {
     *  perror("Can't set baud rate");
     *  return -1;
     * }
     */

    /* Set TTY to N_HCI line discipline
    i = N_HCI;
    if (ioctl(fd, TIOCSETD, &i) < 0) {
        BT_ERROR("Can't set line discipline %d, %s", errno,
               strerror(errno));
        return -1;
    }

    if (flags && ioctl(fd, HCIUARTSETFLAGS, flags) < 0) {
        BT_ERROR("Can't set UART flags %d, %s", errno, strerror(errno));
        return -1;
    }

    if (ioctl(fd, HCIUARTSETPROTO, u->proto) < 0) {
        BT_ERROR("Can't set device %d, %s", errno, strerror(errno));
        return -1;
    }*/

    if (u->post && u->post(fd, u, &ti) < 0)
        return -1;

    return fd;
}

#define OSI_NO_INTR(fn)  do {} while ((fn) == -1 && errno == EINTR)

int uart_write(int fd, void *data, uint32_t size)
{
    int ret;

    OSI_NO_INTR(ret = write(fd, data, size));
    return ret;
}

int uart_read(int fd, void *data, uint32_t size)
{
    int ret;

    OSI_NO_INTR(ret = read(fd, data, size));
    return ret;
}

int uart_readn(int fd, uint8_t *data, uint32_t size)
{
    uint32_t nleft = size;
    int32_t nread;
    struct timeval timeout;
    fd_set readfds;

    if (data == NULL) {
        pr_err("Invalid data buffer\n");
        return -1;
    }

    while (nleft > 0)
    {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0)    // error
        {
            if (errno == EINTR) {
                pr_info("select interrupted by signal, retrying...\n");
                continue;
            }
            pr_err("select error - %s\n", strerror(errno));
            return -1;
        }
        else if (ret == 0)  // timeout
        {
            //pr_info("uart timeout\n");
            return (size - nleft);
        }
        else    // received something
        {
            if ((nread = uart_read(fd, data, nleft)) < 0)
            {
                if (errno == EINTR)
                    continue;
                pr_err("uart_read error - %s\n", strerror(errno));
                return -1;
            }
            else if (nread == 0)
            {
                pr_err("EOF received\n");
                return (size - nleft);
            }
            nleft -= nread;
            data += nread;
        }
    }

    return (size - nleft);
}

void uart_set_hw_fctrl(int fd, uint8_t hw_fctrl)
{
    struct termios termios_old;

    if (fd == -1)
    {
        pr_err("vnd_userial.fd is -1\n");
        return;
    }

    tcgetattr(fd, &termios_old);
    if (hw_fctrl)
    {
        if (termios_old.c_cflag & CRTSCTS)
        {
            pr_info("hw flowcontrol is on\n");
            return;
        }
        else
        {
            termios_old.c_cflag |= CRTSCTS;
            tcsetattr(fd, TCSANOW, &termios_old);
            pr_info("set hw flowcontrol on\n");
        }
    }
    else
    {
        if (termios_old.c_cflag & CRTSCTS)
        {
            termios_old.c_cflag &= ~CRTSCTS;
            tcsetattr(fd, TCSANOW, &termios_old);
            pr_info("userial_vendor_set_hw_fctrl set hw flowcontrol off\n");
            return;
        }
        else
        {
            pr_info("userial_vendor_set_hw_fctrl already hw flowcontrol off\n");
            return;
        }
    }
}





