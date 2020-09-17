/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-08-28     Administrator       the first version
 */
#include "modbus_tcp_rtos.h"

int modbus_tcp_connect(small_modbus_t *smb,char *ip,uint16_t port);
int modbus_tcp_listen(small_modbus_t *smb,uint16_t port);


static int tcp_open(small_modbus_t *smb)
{
    int rc;
    modbus_tcp_config_t *config = smb->port_data;

    config->socket = socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
    smb->port->debug(smb,0,"socket created :%d",config->socket);

//    /* Set send timeout of this fd as per config */
//    config->tv.tv_sec = smb->write_timeout/1000;
//    config->tv.tv_usec = (smb->write_timeout%1000)*1000;
//    setsockopt(config->socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&(config->tv), sizeof(config->tv));
//
//    /* Set recv timeout of this fd as per config */
//    config->tv.tv_sec = smb->read_timeout/1000;
//    config->tv.tv_usec = (smb->read_timeout%1000)*1000;
//    setsockopt(config->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&(config->tv), sizeof(config->tv));

    if(config->isSlave)
    {
         config->socket_addr.sin_family = AF_INET;
         config->socket_addr.sin_port = htons(config->port);
         config->socket_addr.sin_addr.s_addr = htonl(IPADDR_ANY); //inet_addr(ip);

         rc = bind(config->socket, (struct sockaddr *)&(config->socket_addr), sizeof(config->socket_addr));

         smb->port->debug(smb,0,"socket bind :%d\n",rc);

         rc = listen(config->socket, 3);

         smb->port->debug(smb,0,"socket listen :%d\n",rc);
    }else
    {
        config->socket_addr.sin_family = AF_INET;
        config->socket_addr.sin_port = htons(config->port);
        config->socket_addr.sin_addr.s_addr = inet_addr(config->ip);//htonl(IPADDR_BROADCAST);;

        rc = connect(config->socket, (struct sockaddr *)&(config->socket_addr), sizeof(config->socket_addr));

        smb->port->debug(smb,0,"socket connect :%d\n",rc);
    }
    smb->port->debug(smb,0,"open tcp %s : %d\n",inet_ntoa(config->socket_addr.sin_addr.s_addr),ntohs(config->socket_addr.sin_port));
    return 0;
}

static int tcp_close(small_modbus_t *smb)
{
    int rc = 0;
    modbus_tcp_config_t *config = smb->port_data;

    rc = close(config->socket);
    smb->port->debug(smb,0,"socket close :%d\n",rc);

    smb->port->debug(smb,0,"close tcp %s : %d\n",
            inet_ntoa(config->socket_addr.sin_addr.s_addr),
            ntohs(config->socket_addr.sin_port));
    return 0;
}

static int tcp_read(small_modbus_t *smb,uint8_t *data, uint16_t length)
{
    int rc = 0;
    modbus_tcp_config_t *config = smb->port_data;

    rc = recv(config->socket_fd, (char *)data, length, 0);

//    rc = rt_ringbuffer_get(&(config->rx_ring), data, length);
//    //rc = rt_device_read(ctx_config->serial,0,data,length);
//
    if(smb->debug_level == 2)
    {
        int i;
        rt_kprintf("read %d,%d :",rc,length);
        for (i = 0; i < rc; i++)
        {
                rt_kprintf("<%02X>", data[i]);
        }
        rt_kprintf("\n");
    }
    return rc;
}
static int tcp_write(small_modbus_t *smb,uint8_t *data, uint16_t length)
{
//    int rc = 0;
    modbus_tcp_config_t *config = smb->port_data;

    send(config->socket_fd, data, length, 0);
//    if(config->rts_set)
//        config->rts_set(smb,1);
//
//    //write(ctx->fd, data, length);
//
//    rt_device_write(config->dev, 0, data, length);
//
//    if(config->rts_set)
//        config->rts_set(smb,0);
//
    if(smb->debug_level == 2)
    {
        int i;
        rt_kprintf("write %d :",length);
        for (i = 0; i < length; i++)
        {
                rt_kprintf("<%02X>", data[i]);
        }
        rt_kprintf("\n");
    }
    return length;
}
static int tcp_flush(small_modbus_t *smb)
{
    modbus_tcp_config_t *config = smb->port_data;
//    int rc = rt_ringbuffer_data_len(&(config->rx_ring));
//    rt_ringbuffer_reset(&(config->rx_ring));
//    return rc;
    return MODBUS_OK;
}
static int tcp_wait(small_modbus_t *smb,int timeout_ms)
{
    modbus_tcp_config_t *config = smb->port_data;
//    int rc = rt_ringbuffer_data_len(&(config->rx_ring));
//    if(rc>0)
//    {
//        return MODBUS_OK;
//    }
//    rt_sem_control(&(config->rx_sem), RT_IPC_CMD_RESET, RT_NULL);
//    if(rt_sem_take(&(config->rx_sem), timeout_ms) == RT_EOK)
//    {
//        return MODBUS_OK;
//    }else
//    {
//       return MODBUS_TIMEOUT;
//    }
    return MODBUS_OK;
}
static int tcp_debug(small_modbus_t *smb,int level,const char *fmt, ...)
{
    //modbus_tcp_config_t *config = smb->port_data;
    static char log_buf[32];
    if(level <= smb->debug_level)
    {
        va_list args;
        va_start(args, fmt);
        rt_vsnprintf(log_buf, 32, fmt, args);
        va_end(args);
        rt_kprintf(log_buf);
    }
    return 0;
}

small_modbus_port_t _modbus_tcp_rtos_port =
{
    .open =  tcp_open,
    .close = tcp_close,
    .read =  tcp_read,
    .write = tcp_write,
    .flush = tcp_flush,
    .wait =   tcp_wait,
    .debug =  tcp_debug
};

int modbus_tcp_init(small_modbus_t *smb,small_modbus_port_t *port,void *config)
{
    _modbus_init(smb);
    smb->core = (small_modbus_core_t*)&_modbus_tcp_core;
    smb->port_data = config;
    if(port ==NULL)
    {
        smb->port = &_modbus_tcp_rtos_port;
    }else {
        smb->port = port;
    }
    return 0;
}

int modbus_tcp_config(small_modbus_t *smb,uint16_t isSlave,char *ip,uint16_t port)
{
    modbus_tcp_config_t *config = smb->port_data;

    config->isSlave = isSlave;
    config->port = port;

    memset(config->ip,0,16);
    memcpy(config->ip,ip, ((strlen(ip)<=16)?strlen(ip):16));

    return 0;
}

int modbus_tcp_accept(small_modbus_t *smb)
{
    int rc;
    modbus_tcp_config_t *config = smb->port_data;

    struct sockaddr client_addr;
    socklen_t sin_size = sizeof(struct sockaddr);

    rc = accept(config->socket,&client_addr, &sin_size);

    return rc;
}

int modbus_tcp_select(small_modbus_t *smb,int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    int rc;
    //modbus_tcp_config_t *config = smb->port_data;

    rc = select(nfds, readfds, writefds, exceptfds, timeout);

    return rc;
}


int modbus_tcp_set_socket(small_modbus_t *smb,int socket_fd)
{
    modbus_tcp_config_t *config = smb->port_data;
    config->socket_fd = socket_fd;
    return 0;
}
