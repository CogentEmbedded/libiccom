/*
 * Copyright (c) 2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php
 */

#ifndef ICCOM_H
#define ICCOM_H

#ifndef __KERNEL__
#include <stdint.h>
#endif
/*****************************************************************************/
/*  macro definition                                                         */
/*****************************************************************************/
/* channel number */
enum Iccom_channel_number {
	ICCOM_CHANNEL_0 = 0,
	ICCOM_CHANNEL_1,
	ICCOM_CHANNEL_2,
	ICCOM_CHANNEL_3,
	ICCOM_CHANNEL_4,
	ICCOM_CHANNEL_5,
	ICCOM_CHANNEL_6,
	ICCOM_CHANNEL_7,
	ICCOM_CHANNEL_MAX			/* channel maximum count    */
};

/*****************************************************************************/
/*  typedef definition                                                       */
/*****************************************************************************/
/* callback function parameter  */
typedef void (*Iccom_recv_callback_t) (
	enum Iccom_channel_number channel_no,	/* channel number           */
	uint32_t recv_size,			/* receive byte count       */
	uint8_t *recv_buf );			/* data receive buffer      */

/* channel handle */
typedef void* Iccom_channel_t;

/* Iccom_lib_Init parameter */
typedef struct {
	enum Iccom_channel_number channel_no;	/* channel number           */
	uint8_t *recv_buf;			/* data receive buffer      */
	Iccom_recv_callback_t recv_cb;		/* callback function        */
} Iccom_init_param;

/* Iccom_lib_Send parameter */
typedef struct {
	Iccom_channel_t channel_handle;		/* channel handle           */
	uint32_t send_size;			/* send byte count          */
	uint8_t *send_buf;			/* data send buffer         */
} Iccom_send_param;

/*****************************************************************************/
/* function prototype                                                        */
/*****************************************************************************/
/* channel initialization function */
int32_t Iccom_lib_Init(const Iccom_init_param *pIccomInit,
			Iccom_channel_t  *pChannelHandle);

/* channel finalization function */
int32_t Iccom_lib_Final(Iccom_channel_t ChannelHandle);

/* data send function   */
int32_t Iccom_lib_Send(const Iccom_send_param *pIccomSend);

/* API return codes */
#define ICCOM_OK		0	/* Normal completion                */
#define ICCOM_NG		(-1)	/* Abnormal completion              */
#define ICCOM_ERR_PARAM		(-2)	/* Parameter error                  */
#define ICCOM_ERR_BUF_FULL	(-3)	/* Buffer full error                */
#define ICCOM_ERR_TO_ACK	(-4)	/* Acknowledgement timeout error    */
#define ICCOM_ERR_BUSY		(-5)	/* Channel busy                     */
#define ICCOM_ERR_TO_INIT	(-6)	/* Channel initialization error     */
					/* (CR7 side initialization timeout)*/
#define ICCOM_ERR_TO_SEND	(-7)	/* Data send timeout error          */
#define ICCOM_ERR_UNSUPPORT	(-8)	/* Channel unsupported              */
#define ICCOM_ERR_SIZE		(-9)	/* Send size illegal                */

/* communication maximum buffer size */
#define ICCOM_BUF_MAX_SIZE 2048U

#endif /* ICCOM_H */
