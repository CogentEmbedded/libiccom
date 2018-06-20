/*
 * Copyright (c) 2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "iccom.h"
#include "iccom_library.h"

/*****************************************************************************/
/* internal function prototype definition                                    */
/*****************************************************************************/
/* data receive thread  */
static void *iccom_lib_recv_thread(void *arg);

/* channel handle check function */
static int32_t
	iccom_lib_check_handle(const struct iccom_channel_info_t *channel_info,
	uint32_t *channel_no);

#ifdef ICCOM_API_DEBUG
/* channel handle information log function */
static void iccom_lib_handle_log(const int8_t *func_name, int32_t func_line,
	struct iccom_channel_info_t *channel_info,
	uint32_t channel_no);
#endif

/*****************************************************************************/
/* "ICCOM library" global information                                        */
/*****************************************************************************/
/* each channel information */
static struct iccom_channel_global_t
	g_lib_channel_global[ICCOM_CHANNEL_MAX] = {NULL};
/* global mutex information */
static pthread_mutex_t g_lib_mutex_global = PTHREAD_MUTEX_INITIALIZER;

/*****************************************************************************/
/*                                                                           */
/*  Name     : Iccom_lib_Init                                                */
/*  Function : Execute initialization processing of channel communicate.     */
/*             1. Open the channel of Linux ICCOM driver.                    */
/*             2. Create the receive thread.                                 */
/*             3. Create channel handle.                                     */
/*  Callinq seq.                                                             */
/*           Iccom_lib_Init(const Iccom_init_param *pIccomInit,              */
/*                          Iccom_channel_t	   *pChannelHandle)          */
/*  Input    : *pIccomInit     : Channel initialization parameter pointer.   */
/*  Output   : *pChannelHandle : Channel handle pointer.                     */
/*  Return   : 1. ICCOM_OK           (0)  : Normal                           */
/*             2. ICCOM_ERR_PARAM    (-2) : Parameter error                  */
/*             3. ICCOM_ERR_BUSY     (-5) : Channel busy                     */
/*             4. ICCOM_ERR_TO_INIT  (-6) : Initialization error             */
/*             5. ICCOM_ERR_UNSUPPORT(-8) : Unsupported channel              */
/*             6. ICCOM_NG           (-1) : Other error                      */
/*  Caller   : Application                                                   */
/*                                                                           */
/*****************************************************************************/
int32_t Iccom_lib_Init(const Iccom_init_param *pIccomInit,
			Iccom_channel_t *pChannelHandle)
{
	struct iccom_channel_info_t *l_channel_info = NULL; /* channel handle*/
	struct iccom_channel_global_t *channel_global; /* ch. global pointer */
	int32_t l_fd = (-1);			/* file descriptor           */
	int32_t retcode = ICCOM_OK;		/* return code               */
	int32_t ret;				/* call function return code */
	uint32_t l_channel_no;			/* channel number            */
	int8_t devname[ICCOM_DEVFILE_LEN] = {'\0'};  /* device file name area*/
	uint8_t  mutexflg = ICCOM_LIB_OFF;      /* mutex initialized flag    */

	LIBPRT_DBG("start : pIccomInit   = %p", (const void *)pIccomInit);

	/* check parameter pointer */
	if (pIccomInit == NULL) {
		LIBPRT_ERR("parameter none");
		retcode = ICCOM_ERR_PARAM;
	}

	if (retcode == ICCOM_OK) {
		LIBPRT_DBG("channel_no = %d",
			(int32_t)pIccomInit->channel_no);
		LIBPRT_DBG("recv_buf   = %p", (void *)pIccomInit->recv_buf);
		LIBPRT_DBG("recv_cb    = %p", (void *)pIccomInit->recv_cb);
		LIBPRT_DBG("recv_thread = %p", (void *)iccom_lib_recv_thread);

		l_channel_no = (uint32_t)pIccomInit->channel_no;
		/* check initialization parameter contents */
		if ((pIccomInit->recv_buf == NULL) ||
		    (pIccomInit->recv_cb == NULL) ||
		    (l_channel_no >= (uint32_t)ICCOM_CHANNEL_MAX)) {
			LIBPRT_ERR(
				"parameter err : recv_buf = %p, recv_cb = %p,"
				" channel No. = %d",
				(void *)pIccomInit->recv_buf,
				(void *)pIccomInit->recv_cb, l_channel_no);
			retcode = ICCOM_ERR_PARAM;
		}
	}

	if (retcode == ICCOM_OK) {
		/* create device file name */
		LIBPRT_DBG("snprintf para : 2nd = %u, 4th = %s, 5th = %u",
			ICCOM_DEVFILE_LEN, ICCOM_DEVFILENAME, l_channel_no);
		ret = snprintf((char *)devname, ICCOM_DEVFILE_LEN,
			"%s%d", ICCOM_DEVFILENAME, l_channel_no);
		if (ret < 0) {
			LIBPRT_ERR(
				"cannot create device file name : err = %d",
				ret);
			retcode = ICCOM_NG;
		}
	}

	if (retcode == ICCOM_OK) {
		LIBPRT_NRL("device file name = %s, O_RDWR = %d",
			   devname, O_RDWR);
		/* open channel */
		l_fd = open((char *)devname, O_RDWR);
		LIBPRT_NRL("open channel: retcode = %d", l_fd);
		if (l_fd < 0) {
			switch (errno) {
			case EBUSY:
				retcode = ICCOM_ERR_BUSY;
				break;
			case EDEADLK:
				retcode = ICCOM_ERR_TO_INIT;
				break;
			case ENOENT:
			case ENODEV:
			case ENXIO:
				retcode = ICCOM_ERR_UNSUPPORT;
				break;
			default:
				retcode = ICCOM_NG;
				break;
			}
			LIBPRT_ERR(
				"open err : channel No. = %d, errno = %d:%s,"
				" return code = %d",
				l_channel_no, errno, strerror(errno), retcode);
		}
	}

	if (retcode == ICCOM_OK) {
		channel_global = &g_lib_channel_global[l_channel_no];
		/* get channel handle information area */
		LIBPRT_DBG("malloc para = %lu", sizeof(*l_channel_info));
		l_channel_info = (struct iccom_channel_info_t *)malloc(
			sizeof(*l_channel_info));
		if (l_channel_info == NULL) {
			LIBPRT_ERR("cannot get channel handle area");
			retcode = ICCOM_NG;
		}
		LIBPRT_DBG("l_channel_info = %p", (void *)l_channel_info);
	}

	if (retcode == ICCOM_OK) {
		/* initialize channel handle information area */
		LIBPRT_DBG("memset para : 1st = %p, 3rd =%lu",
			(void *)l_channel_info, sizeof(*l_channel_info));
		(void)memset(
			(void *)l_channel_info, 0, sizeof(*l_channel_info));

		/* initial setting channel handle information */
		l_channel_info->channel_no = pIccomInit->channel_no;
		l_channel_info->send_req_cnt = 0U;
		l_channel_info->recv_buf = pIccomInit->recv_buf;
		l_channel_info->recv_cb = pIccomInit->recv_cb;
		l_channel_info->fd = l_fd;

		/* initialize channel mutex information */
		LIBPRT_DBG("channel pthread_mutex_init para = %p",
			(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_init(
				&channel_global->mutex_channel_info, NULL);
		mutexflg = ICCOM_LIB_ON;

		/* create data receive thread */
		LIBPRT_DBG("pthread_create in para : 3nd  = %p, 4th= %p",
			(void *)iccom_lib_recv_thread, (void *)l_channel_info);
		ret = pthread_create(
			&l_channel_info->recv_thread_id, NULL,
			iccom_lib_recv_thread, (void *)l_channel_info);
		LIBPRT_DBG("pthread_create out 1st para : para = %p",
			   (void *)l_channel_info->recv_thread_id);
		if (ret != 0) {
			LIBPRT_ERR(
				"receive thread creation err : err = %d", ret);
			retcode = ICCOM_NG;
		}
	}

	if (retcode == ICCOM_OK) {
		/* lock global mutex */
		LIBPRT_DBG("pthread_mutex_lock para = %p",
			(void *)&g_lib_mutex_global);
		(void)pthread_mutex_lock(&g_lib_mutex_global);
		/* set channel handle pointer */
		channel_global->channel_info = l_channel_info;
		/* unlock global mutex */
		LIBPRT_DBG("pthread_mutex_unlock para = %p",
			&g_lib_mutex_global);
		(void)pthread_mutex_unlock(&g_lib_mutex_global);

		/* set channel handle for application */
		*pChannelHandle = (Iccom_channel_t)l_channel_info;

		/* output channel handle debug log */
		LIB_CANANEL_HANDLE_DBGLOG(l_channel_info, l_channel_no);

	} else {
		/* abnormal correspondence */
		/* channel opened already */
		if (l_fd >= 0) {
			LIBPRT_DBG("close para = %d", l_fd);
			(void)close(l_fd);
		}
		/* channel mutex initialized already */
		if (mutexflg == ICCOM_LIB_ON) {
			LIBPRT_DBG("channel pthread_mutex_destroy para = %p",
				(void *)&channel_global->mutex_channel_info);
			(void)pthread_mutex_destroy(
				&channel_global->mutex_channel_info);
		}
		/* memory allocated already */
		if (l_channel_info != NULL) {
			LIBPRT_DBG("free para = %p", (void *)l_channel_info);
			free(l_channel_info);
		}
	}
	LIBPRT_DBG("end : retcode = %d", retcode);
	return retcode;
}


/*****************************************************************************/
/*                                                                           */
/*  Name     : Iccom_lib_Send                                                */
/*  Function : Send data from Linux side to CR7 side.                        */
/*  Callinq seq.                                                             */
/*           Iccom_lib_Send(const Iccom_send_param *pIccomSend)              */
/*  Input    : * pIccomSend : The send parameter pointer.                    */
/*  Return   : 1. ICCOM_OK           (0)  : Normal                           */
/*             2. ICCOM_ERR_PARAM    (-2) : Parameter error                  */
/*             3. ICCOM_ERR_BUF_FULL (-3) : Buffer full error                */
/*             4. ICCOM_ERR_TO_ACK   (-4) : Acknowledgement timeout erorr    */
/*             5. ICCOM_ERR_TO_SEND  (-7) : Data send timeout error          */
/*             6: ICCOM_ERR_SIZE     (-9) : Send size illegal                */
/*             7. ICCOM_NG           (-1) : Other error                      */
/*  Caller   : Application                                                   */
/*  Note     : Use of channel number in this function is necessary to use    */
/*             value obtained in call of iccom_lib_check_handle function.    */
/*                                                                           */
/*****************************************************************************/
int32_t Iccom_lib_Send(const Iccom_send_param *pIccomSend)
{
	struct iccom_channel_info_t *l_channel_info;   /* channel handle inf.*/
	struct iccom_channel_global_t *channel_global; /* ch. global pointer */
	ssize_t write_count;			/* send size(result)         */
	int32_t retcode = ICCOM_OK;		/* return code               */
	int32_t ret;				/* call function return code */
	uint32_t l_channel_no;			/* channel number            */
	uint8_t req_update_flag = ICCOM_LIB_OFF; /* req. counter update flag */

	LIBPRT_DBG("start : pIccomSend = %p", (const void *)pIccomSend);

	/* check parameter pointer */
	if (pIccomSend == NULL) {
		LIBPRT_ERR("parameter none");
		retcode = ICCOM_ERR_PARAM;
	}

	if (retcode == ICCOM_OK) {
		LIBPRT_DBG("send_size  = %d", pIccomSend->send_size);
		LIBPRT_DBG("send_buf   = %p", (void *)pIccomSend->send_buf);

		/* check send parameter contents */
		if ((pIccomSend->send_size > ICCOM_BUF_MAX_SIZE) ||
		    (pIccomSend->send_buf == NULL)) {
			LIBPRT_ERR(
				"parameter err : send_size = %u,"
				" send_buf = %p",
				pIccomSend->send_size,
				(void *)pIccomSend->send_buf);
			retcode = ICCOM_ERR_PARAM;

		}
	}

	if (retcode == ICCOM_OK) {
		l_channel_info = (struct iccom_channel_info_t *)
			 pIccomSend->channel_handle;
		/* check channel handle & get channel number */
		LIBPRT_DBG("iccom_lib_check_handle para  1st = %p, 2nd = %p",
				(void *)l_channel_info, (void *)&l_channel_no);
		ret = iccom_lib_check_handle(l_channel_info, &l_channel_no);
		LIBPRT_DBG("iccom_lib_check_handle ret= %d ,l_channel_no = %u",
			    ret, l_channel_no);
		if (ret != ICCOM_OK) {
			LIBPRT_ERR("channel handle err : err = %d", ret);
			retcode = ret;
		}
	}

	if (retcode == ICCOM_OK) {
		channel_global = &g_lib_channel_global[l_channel_no];

		/* lock channel handle */
		LIBPRT_DBG("pthread_mutex_lock para = %p",
				(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_lock(&channel_global->mutex_channel_info);

		/* output channel handle debug log */
		LIB_CANANEL_HANDLE_DBGLOG(l_channel_info, l_channel_no);

		/* check channel handle pointer of global */
		if (channel_global->channel_info == NULL) {
			LIBPRT_ERR("channel not open : channel No. = %u",
				l_channel_no);
			LIBPRT_DBG("pthread_mutex_unlock para = %p",
				(void *)&channel_global->mutex_channel_info);
			(void)pthread_mutex_unlock(
				&channel_global->mutex_channel_info);
			retcode = ICCOM_ERR_PARAM;
		}
	}

	if (retcode == ICCOM_OK) {
		/* increment send request counter */
		l_channel_info->send_req_cnt++;
		req_update_flag = ICCOM_LIB_ON;

		/* unlock channel handle */
		LIBPRT_DBG("pthread_mutex_unlock para = %p",
				(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_unlock(
			&channel_global->mutex_channel_info);

		/* send data */
		LIBPRT_DBG("write function para : 1st = %d, 2nd = %p, 3rd = %u",
			    l_channel_info->fd, (void *)pIccomSend->send_buf,
			    pIccomSend->send_size);
		write_count = write(l_channel_info->fd, pIccomSend->send_buf,
				(size_t)pIccomSend->send_size);
		/* output channel handle debug log */
		LIB_CANANEL_HANDLE_DBGLOG(l_channel_info, l_channel_no);
		LIBPRT_NRL("send data : send size(result) = %ld", write_count);
		if (write_count != (ssize_t)pIccomSend->send_size) {
			if (write_count < 0)  {
				/* abnormal end */
				switch (errno) {
				case ENOSPC:
					retcode = ICCOM_ERR_BUF_FULL;
					break;
				case ETIMEDOUT:
					retcode = ICCOM_ERR_TO_ACK;
					break;
				case EDEADLK:
					retcode = ICCOM_ERR_TO_SEND;
					break;
				default:
					retcode = ICCOM_NG;
				break;
				}
				LIBPRT_ERR(
					"send err : channel No. = %d,"
					" errno = %d:%s, return code = %d",
					l_channel_no, errno, strerror(errno),
					retcode);
			}
			/* illegal send size */
			else {
				LIBPRT_ERR(
					"send size mismatch : channel No. = %d,"
					"request size = %d, result size = %ld",
					l_channel_no, pIccomSend->send_size,
					write_count);
				retcode = ICCOM_ERR_SIZE;
			}
		}
	}

	/* check send request counter increment */
	if (req_update_flag == ICCOM_LIB_ON) {
		LIBPRT_DBG("send count decrement");

		/* lock channel handle */
		LIBPRT_DBG("pthread_mutex_lock para = %p",
			(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_lock(&channel_global->mutex_channel_info);
		/* decrement send request counter */
		l_channel_info->send_req_cnt--;
		/* unlock channel handle */
		LIBPRT_DBG("pthread_mutex_unlock para = %p",
			(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_unlock(
			&channel_global->mutex_channel_info);

		/* output channel handle debug log */
		LIB_CANANEL_HANDLE_DBGLOG(l_channel_info, l_channel_no);
	}

	LIBPRT_DBG("end : retcode = %d", retcode);
	return retcode;
}

/*****************************************************************************/
/*                                                                           */
/*  Name     : Iccom_lib_Final                                               */
/*  Function : Execute finalization processing of channel communication.     */
/*             1. End the receive thread.                                    */
/*             2. Close the channel of Linux ICCOM driver.                   */
/*             3. Release channel handle.                                    */
/*  Callinq seq.                                                             */
/*           Iccom_lib_Final(Iccom_channel_t ChannelHandle)                  */
/*  Input    : *ChannelHandle  : Channel handle                              */
/*  Return   : 1. ICCOM_OK           (0)  : Normal                           */
/*             2. ICCOM_ERR_PARAM    (-2) : Parameter error                  */
/*             3. ICCOM_NG           (-1) : 1 Data sending                   */
/*                                        : 2 data receiving                 */
/*                                          3 other                          */
/*  Caller   : Application                                                   */
/*  Note     : Use of channel number in this function is necessary to use    */
/*             value obtained in call of iccom_lib_check_handle function.    */
/*                                                                           */
/*****************************************************************************/
int32_t Iccom_lib_Final(Iccom_channel_t ChannelHandle)
{
	struct iccom_channel_info_t *l_channel_info;   /* channel handle inf.*/
	struct iccom_channel_global_t *channel_global; /* ch. global pointer */
	int32_t retcode = ICCOM_OK;		/* return code               */
	int32_t ret;				/* call function return code */
	uint32_t l_channel_no;			/* channel number            */
	uint8_t channel_mutex_flag = ICCOM_LIB_OFF; /* channel mutex flag    */

	LIBPRT_DBG("start : ChannelHandle = %p", ChannelHandle);

	l_channel_info = (struct iccom_channel_info_t *)ChannelHandle;
	/* check channel handle & get channel number */
	LIBPRT_DBG("iccom_lib_check_handle para  1st = %p, 2nd = %p",
		   (void *)l_channel_info, (void *)&l_channel_no);
	ret = iccom_lib_check_handle(l_channel_info, &l_channel_no);
	LIBPRT_DBG("iccom_lib_check_handle ret = %d ,l_channel_no = %u",
		    ret, l_channel_no);
	if (ret != ICCOM_OK) {
		LIBPRT_ERR("channel handle err : err = %d", ret);
		retcode = ret;
	}

	if (retcode == ICCOM_OK) {
		channel_global = &g_lib_channel_global[l_channel_no];

		/* lock channel handle */
		LIBPRT_DBG("pthread_mutex_lock para = %p",
			(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_lock(&channel_global->mutex_channel_info);
		channel_mutex_flag = ICCOM_LIB_ON;

		/* output channel handle debug log */
		LIB_CANANEL_HANDLE_DBGLOG(l_channel_info, l_channel_no);

		/* check channel handle pointer of global */
		if (channel_global->channel_info == NULL) {
			LIBPRT_ERR("channel not open : channel No. = %u",
				l_channel_no);
			retcode = ICCOM_ERR_PARAM;
		}
	}

	if (retcode == ICCOM_OK) {
		/*  check data sending now */
		if (l_channel_info->send_req_cnt != 0U) {
			LIBPRT_ERR(
				"data sending : channel No. = %u,"
				" send request count = %u\n",
				l_channel_no, l_channel_info->send_req_cnt);
			retcode = ICCOM_ERR_PARAM;
		}
	}

	if (retcode == ICCOM_OK) {
		/* End the data receive(Execute ioctl) */
		LIBPRT_DBG("ioctl function para 1st = %d, 2nd = %d",
			   l_channel_info->fd, ICCOM_IOC_CANCEL_RECEIVE);
		ret = ioctl(l_channel_info->fd, ICCOM_IOC_CANCEL_RECEIVE, NULL);
		LIBPRT_NRL("ioctl : retcode = %x", ret);
		if (ret != 0) {
			retcode = ICCOM_NG;
			LIBPRT_ERR("ioctl : channel No. = %d,"
				" errno = %d:%s, return code = %d",
				l_channel_no, errno, strerror(errno),
				retcode);
		}
	}

	if (retcode == ICCOM_OK) {
		/* wait receive thread end */
		LIBPRT_DBG("pthread_join = %lu", l_channel_info->recv_thread_id);
		(void)pthread_join(l_channel_info->recv_thread_id, NULL );

		/* close channel */
		LIBPRT_DBG("close function para = %d", l_channel_info->fd);
		(void)close(l_channel_info->fd);
		LIBPRT_NRL("close channel : retcode = %x", ret);

		/* lock global mutex */
		LIBPRT_DBG("pthread_mutex_lock para = %p",
			(void *)&g_lib_mutex_global);
		(void)pthread_mutex_lock(&g_lib_mutex_global);

		/* clear channel handle pointer */
		channel_global->channel_info = NULL;

		/* output channel handle debug log */
		LIB_CANANEL_HANDLE_DBGLOG(l_channel_info, l_channel_no);

		/* free channel handle */
		LIBPRT_DBG("free para = %p", (void *)l_channel_info);
		free(l_channel_info);

		/* unlock global mutex */
		LIBPRT_DBG("pthread_mutex_unlock para = %p",
			(void *)&g_lib_mutex_global);
		(void)pthread_mutex_unlock(&g_lib_mutex_global);

		/* unlock channel handle & delete mutex */
		LIBPRT_DBG("pthread_mutex_unlock para = %p",
			(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_unlock(
			&channel_global->mutex_channel_info);
		LIBPRT_DBG("pthread_mutex_destroy para = %p",
			(void *)&channel_global->mutex_channel_info);
		(void)pthread_mutex_destroy(
			&channel_global->mutex_channel_info);

	} else {
		/* channel mutex lock already */
		if (channel_mutex_flag == ICCOM_LIB_ON) {
			/* channel mutex unlock */
			LIBPRT_DBG("pthread_mutex_unlock para = %p",
				(void *)&channel_global->mutex_channel_info);
			(void)pthread_mutex_unlock(
				&channel_global->mutex_channel_info);
		}
	}
	LIBPRT_DBG("end : retcode = %d", retcode);
	return retcode;
}

/*****************************************************************************/
/*                                                                           */
/*  Name     : iccom_lib_recv_thread                                         */
/*  Function : 1. Receive data from CR7 side.                                */
/*             2. Call callback function for pass the received data.         */
/*  Callinq seq.                                                             */
/*           iccom_lib_recv_thread(void *arg)                                */
/*  Input    : *arg            : Channel handle infomation                   */
/*  return   : NULL                                                          */
/*  Note     : This thread is created in execution of pthread_create         */
/*             function in iccom_lib_init. In addition, to end in the        */
/*             execution of pthread_cancel in iccom_lib_final.               */
/*                                                                           */
/*****************************************************************************/
static void *iccom_lib_recv_thread(void *arg)
{
	struct iccom_channel_info_t *l_channel_info; /* channel handle info. */
	ssize_t read_size;			/* receive size(result)      */

	l_channel_info = (struct iccom_channel_info_t *)arg;

	LIBPRT_DBG("start : fd=%d, callback=%p, channel No.=%d, recv_buf=%p",
		l_channel_info->fd, (void *)l_channel_info->recv_cb,
		(int32_t)l_channel_info->channel_no,
		(void *)l_channel_info->recv_buf);
	while (1) {
		/* receive data */
		LIBPRT_DBG("read function para : 1st = %d, 2nd = %p, 3rd = %u",
			    l_channel_info->fd,
			    (void *)l_channel_info->recv_buf,
			    ICCOM_BUF_MAX_SIZE);
		read_size = read(l_channel_info->fd, l_channel_info->recv_buf,
			ICCOM_BUF_MAX_SIZE);
		LIBPRT_NRL("receive data : receive size = %ld, errno = %d",
			   read_size, errno);
		if (read_size >= 0) {
			LIBPRT_DBG(
				"call callback function : call back = %p"
				" channel No. = %d, size = %ld, buf = %p",
				(void *)l_channel_info->recv_cb,
				(int32_t)l_channel_info->channel_no,
				read_size, (void *)l_channel_info->recv_buf);
			/* call callback function */
			(*l_channel_info->recv_cb)(
				l_channel_info->channel_no,
				(uint32_t)read_size,
				l_channel_info->recv_buf);
		} else {
			/* end data receive */
			if (errno == ECANCELED) {
				break;
			}
			/* otter */
			LIBPRT_ERR(
				"receive err : channel No. = %d, err = %d:%s",
				(int32_t)l_channel_info->channel_no,
				errno, strerror(errno));
		}
	}
	LIBPRT_DBG("end");
	pthread_exit(NULL);
}

/*****************************************************************************/
/*                                                                           */
/*  Name     : iccom_lib_check_handle                                        */
/*  Function : Check channel handle and get channel number                   */
/*  Callinq seq.                                                             */
/*           iccom_lib_check_handle(                                         */
/*                           struct iccom_channel_info_t *channel_info,      */
/*                           uint32_t                    *channel_no)        */
/*  Input    : *channel_info   : Channel handle pointer.                     */
/*  Output   : *channel_no     : channel number pointer.                     */
/*  Return   : 1. ICCOM_OK           (0)  : Normal                           */
/*             2. ICCOM_ERR_PARAM    (-2) : Parameter error                  */
/*  Caller   : Iccom_lib_Send, Iccom_lib_Final                               */
/*                                                                           */
/*****************************************************************************/
static int32_t iccom_lib_check_handle(
	const struct iccom_channel_info_t *channel_info, uint32_t *channel_no)
{
	int32_t retcode = ICCOM_OK;	/* return code                       */
	uint32_t ch_loop;		/* loop counter of channel number    */

	LIBPRT_DBG("start channel_info = %p", (const void *)channel_info);

	/* check channel handle pointer(client side channel handle) */
	if (channel_info == NULL) {
		LIBPRT_ERR("channel handle none");
		retcode = ICCOM_ERR_PARAM;
	}

	if (retcode == ICCOM_OK) {
		/* lock global mutex */
		LIBPRT_DBG("pthread_mutex_lock para = %p",
			(void *)&g_lib_mutex_global);
		(void)pthread_mutex_lock(&g_lib_mutex_global);

		/* search channel handle pointer channel handle pointer */
		for (ch_loop = 0U; ch_loop < (uint32_t)ICCOM_CHANNEL_MAX;
		     ch_loop++) {
			/* check channel handle pointer */
			if (g_lib_channel_global[ch_loop].channel_info ==
				channel_info) {
				break;
			}
		}

		/* unlock global mutex */
		LIBPRT_DBG("pthread_mutex_unlock para = %p",
			(void *)&g_lib_mutex_global);
		(void)pthread_mutex_unlock(&g_lib_mutex_global);

		/* check loop count */
		if (ch_loop >= (uint32_t)ICCOM_CHANNEL_MAX) {
			/* not found channel handle pointer */
			LIBPRT_ERR("not found channel handle pointer");
			retcode = ICCOM_ERR_PARAM;
		}
	}

	if (retcode == ICCOM_OK) {
		/* check channel number */
		if ((uint32_t)channel_info->channel_no != ch_loop) {
			LIBPRT_ERR(
				"mismatch channel No. : handle = %u,"
				" global = %u",
				(uint32_t)channel_info->channel_no, ch_loop);
			retcode = ICCOM_ERR_PARAM;
		}
	}

	if (retcode == ICCOM_OK) {
		/* set channel number */
		*channel_no = (uint32_t)channel_info->channel_no;
	}

	LIBPRT_DBG("end:channel No. %d, retcode = %d, loop_connter %u",
		*channel_no, retcode, ch_loop);
	return retcode;
}

#ifdef ICCOM_API_DEBUG
/*****************************************************************************/
/*                                                                           */
/*  Name     : iccom_lib_handle_log                                          */
/*  Function : Log channel handle information.                               */
/*  Callinq seq.                                                             */
/*	       iccom_lib_handle_log(const int8_t *func_name,                 */
/*                                int8_t *func_line,                         */
/*                                struct iccom_channel_info_t *channel_info, */
/*                                uint32_t channel_no)                       */
/*  Input    : *func_name      : function name pointer                       */
/*             func_line       : Line number                                 */
/*             *channel_info   : Channel handle information pointer.         */
/*             channel_no      : Channel number                              */
/*  Return   : NON                                                           */
/*  Caller  : The function in iccom_library.c                                */
/*                                                                           */
/*****************************************************************************/
static void iccom_lib_handle_log(const int8_t *func_name, int32_t func_line,
			struct iccom_channel_info_t *channel_info,
			uint32_t channel_no)
{
	(void)printf("%s() L%d g_channel_no = %d\n",
		func_name, func_line, channel_no);
	(void)printf("g_ch[0]-[3] = %16p %16p %16p %16p\n",
		(void *)g_lib_channel_global[0].channel_info,
		(void *)g_lib_channel_global[1].channel_info,
		(void *)g_lib_channel_global[2].channel_info,
		(void *)g_lib_channel_global[3].channel_info);
	(void)printf("g_ch[4]-[7] = %16p %16p %16p %16p\n",
		(void *)g_lib_channel_global[4].channel_info,
		(void *)g_lib_channel_global[5].channel_info,
		(void *)g_lib_channel_global[6].channel_info,
		(void *)g_lib_channel_global[7].channel_info);

	if (g_lib_channel_global[channel_no].channel_info != NULL) {
		(void)printf("    channel_no = %d\n",
			(int32_t)channel_info->channel_no);
		(void)printf("    send_cnt   = %d\n",
			channel_info->send_req_cnt);
		(void)printf("    recv_buf   = %p\n",
			(void *)channel_info->recv_buf);
		(void)printf("    recv_cb    = %p\n",
			(void *)channel_info->recv_cb);
		(void)printf("    fd         = %d\n", channel_info->fd);
		(void)printf("    thread_id  = %lu\n",
			                  channel_info->recv_thread_id);
	}

	(void)printf("%s() L%d mutex_channel_info = %d\n",
		func_name, func_line, channel_no);
	(void)printf("g_ch_mutex[0]-[3] = %16p %16p %16p %16p\n",
		(void *)&g_lib_channel_global[0].mutex_channel_info,
		(void *)&g_lib_channel_global[1].mutex_channel_info,
		(void *)&g_lib_channel_global[2].mutex_channel_info,
		(void *)&g_lib_channel_global[3].mutex_channel_info);
	(void)printf("g_ch_mutex[4]-[7] = %16p %16p %16p %16p\n",
		(void *)&g_lib_channel_global[4].mutex_channel_info,
		(void *)&g_lib_channel_global[5].mutex_channel_info,
		(void *)&g_lib_channel_global[6].mutex_channel_info,
		(void *)&g_lib_channel_global[7].mutex_channel_info);
	(void)printf("%s() L%d g_lib_mutex_global = %p\n",
		func_name, func_line, (void *)&g_lib_mutex_global);
}
#endif
