/*
 * Copyright (c) 2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php
 */

#ifndef ICCOM_LIBRARY_H
#define ICCOM_LIBRARY_H

#include <pthread.h>
#include "iccom.h"

/*****************************************************************************/
/* define definition                                                         */
/*****************************************************************************/
#define ICCOM_DEVFILENAME "/dev/iccom"	  /* device file name fixed portion  */
#define ICCOM_DEVFILE_LEN (16U)		  /* device file name maximum length */

#define ICCOM_LIB_ON  (1U)		  /* flag ON                         */
#define ICCOM_LIB_OFF (0U)		  /* flag OFF                        */

/*****************************************************************************/
/* structure definition                                                      */
/*****************************************************************************/
/* channel handle information */
struct iccom_channel_info_t {
	enum Iccom_channel_number channel_no;	/* channel number            */
	uint32_t send_req_cnt;			/* send request counter      */
	uint8_t *recv_buf;			/* data receive buffer       */
	Iccom_recv_callback_t recv_cb;		/* callback function         */
	int    fd;				/* file descriptor           */
	pthread_t recv_thread_id;		/* data receive thread ID    */
};

/* channel global information */
struct iccom_channel_global_t {
	struct iccom_channel_info_t *channel_info;	/* channel handle inf*/
	pthread_mutex_t mutex_channel_info;		/* mutex information */
};

/* ioctl request command */
#define ICCOM_IOC_CANCEL_RECEIVE	(1U)          /* Receive end specified */

/*****************************************************************************/
/* LOG definition                                                            */
/*****************************************************************************/
/* error log definition */
#ifdef ICCOM_API_ERROR
#define LIBPRT_ERR(fmt, ...) \
	(void)printf("[ERR]%s() : "fmt"\n", __func__, ## __VA_ARGS__)
#else
#define LIBPRT_ERR(fmt, ...)
#endif


/* normal log definition */
#ifdef ICCOM_API_NORMAL
#define LIBPRT_NRL(fmt, ...) \
	(void)printf("[NML]%s() L%d: "fmt"\n", \
		__func__, __LINE__, ## __VA_ARGS__)
#else
#define LIBPRT_NRL(fmt, ...)
#endif


/* debug log definition */
#ifdef ICCOM_API_DEBUG
#define LIBPRT_DBG(fmt, ...) \
	(void)printf("[DBG]%s() L%d: "fmt"\n", \
		__func__, __LINE__, ## __VA_ARGS__)
#else
#define LIBPRT_DBG(fmt, ...)
#endif


/* channel handle debug log definition */
#ifdef ICCOM_API_DEBUG
#define LIB_CANANEL_HANDLE_DBGLOG(CHANNEL_INFO, CHANNEL_NO) \
	iccom_lib_handle_log((const int8_t *)__func__, __LINE__, \
			(CHANNEL_INFO), (CHANNEL_NO))
#else
#define LIB_CANANEL_HANDLE_DBGLOG(CHANNEL_INFO, CHANNEL_NO) (void)0
#endif

#endif /* ICCOM_LIBRARY_H */
