/*
 * http.c
 *
 *  Created on: 2014Äê6ÔÂ24ÈÕ
 *      Author: Administrator
 */
#include "../cc3200_common.h"

#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "../misc/audio_spi/vs1053b.h"

#define POST_BUFFER_OLD     " HTTP/1.1 \r\nAccept: application/xml,application/xhtml+xml,text/html,*/*;q=0.5\r\nConnection:keep-alive\r\nUser-Agent:Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US)\r\n"
#define HTTP_FILE_NOT_FOUND    "404 Not Found" /* HTTP file not found response */
#define HTTP_STATUS_OK         "200 OK"  /* HTTP status ok response */
#define HTTP_CONTENT_LENGTH    "Content-Length:"  /* HTTP content length header */
#define HTTP_CONTENT_TYPE		"Content-type:"
#define HTTP_TRANSFER_ENCODING "Transfer-Encoding:" /* HTTP transfer encoding header */
#define HTTP_ENCODING_CHUNKED  "chunked" /* HTTP transfer encoding header value */
#define HTTP_CONNECTION        "Connection:" /* HTTP Connection header */
#define HTTP_CONNECTION_CLOSE  "close"  /* HTTP Connection header value */

#define HTTP_END_OF_HEADER  "\r\n\r\n"  /* string marking the end of headers in response */

#define MAX_BUFF_SIZE  1460

static int CreateConnection(unsigned long DestinationIP)
{
    SlSockAddrIn_t  Addr;
    int             Status = 0;
    int             AddrSize = 0;
    int             SockID = 0;

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(80);
    Addr.sin_addr.s_addr = sl_Htonl(DestinationIP);

    AddrSize = sizeof(SlSockAddrIn_t);

    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    ASSERT_ON_ERROR(SockID);

    Status = sl_Connect(SockID, ( SlSockAddr_t *)&Addr, AddrSize);
    if( Status < 0 )
    {
        /* Error */
        sl_Close(SockID);
        ASSERT_ON_ERROR(Status);
    }
    return SockID;
}

static int get_mp3(char *buff, char songs[][128], int max)
{
    char *ptr;
    char *end;
    int i,j,k,index;
    char b[256] = {0};

    ptr = buff;

	index = 0;

	memset(b, '\0', sizeof(b));

	while(strlen(songs[index]) && index < max)
		index++;
    while(index < max)
    {
		ptr = strstr(ptr, "url\":");

		end = strstr(ptr, "mp3");

		if(ptr == 0 || end == 0)
			break;
		j = end - ptr;
		ptr += 6;
		k = 0;
		for(i = 0; i < j - 3; i++)
		{
			if (*(ptr + i) == '\\')
			{
				k++;

			}
			else
				b[i-k] = *(ptr + i);
		}
		memcpy(songs[index++], b, strlen(b));
		//UART_PRINT("[paras: %s]\r\n", b);
    }
    //UART_PRINT("paras over\r\n");
    return index;
}

int play_song(char *req)
{
	int  transfer_len = 0;
	long retVal = 0;

	char *pBuff = 0;
	char *end = 0;

	char g_buff[MAX_BUFF_SIZE] = {0};
	char s_buff[512] = {0};
	char host[64] = {0};

	int g_iSockID;
	unsigned long ip;
	int i;
	int nfds;
	SlFdSet_t readsds;
	struct SlTimeval_t timeout;

	pBuff = strstr(req, "http://");
	if (pBuff)
	{
		pBuff = req + 7;
	}
	else
	{
		pBuff = req;
	}

	end = strstr(pBuff, "/");

	if (!end)
	{
		UART_PRINT("Cannot get host name \r\n");
		return -1;
	}

	for(i = 0; i + pBuff < end; i++ )
	{
		host[i] = *(pBuff + i);
	}

	host[i+1] = '\0';
	pBuff = end;

	retVal = sl_NetAppDnsGetHostByName((signed char *)host, strlen(host), &ip, SL_AF_INET);
    if(retVal < 0)
    {
    	UART_PRINT("Cannot get host %s: %d\r\n", host, ip);
    	return -1;
    }

    g_iSockID = CreateConnection(ip);

	if(g_iSockID < 0)
	{
		UART_PRINT("open socket failed\r\n");
		return -1;
	}

	memset(s_buff, 0, sizeof(s_buff));

	strcat(s_buff, "GET ");
	strcat(s_buff, pBuff);
	strcat(s_buff, POST_BUFFER_OLD);
	strcat(s_buff, "Host: ");
	strcat(s_buff, host);
	strcat(s_buff, "\r\n");
	strcat(s_buff, HTTP_END_OF_HEADER);

	// Send the HTTP GET string to the opened TCP/IP socket.
    transfer_len = sl_Send(g_iSockID, s_buff, strlen((const char *)s_buff) + 1, 0);

	if (transfer_len < 0)
	{
		UART_PRINT("Socket Send Error\r\n");
		sl_Close(g_iSockID);
		return -1;
	}

	audio_play_start();

	memset(g_buff, 0, sizeof(g_buff));
	// Recv HTTP Header.
	transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);

	if(transfer_len > 0)
	{
		// Check for 404 return code
		if(strstr((const char *)g_buff, HTTP_FILE_NOT_FOUND) != 0)
		{
			UART_PRINT("[HTTP] 404\r\n");

			goto end;
		}

		// if not "200 OK" return error
		if(strstr((const char *)g_buff, HTTP_STATUS_OK) == 0)
		{
			UART_PRINT("[HTTP] no 200; %d\r\n", transfer_len);

			goto end;
		}

		pBuff = strstr((const char *)g_buff, HTTP_END_OF_HEADER);

		while(pBuff == NULL)											// XXX
		{
			memset(g_buff, 0, sizeof(g_buff));

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			SL_FD_ZERO(&readsds);
			SL_FD_SET(g_iSockID, &readsds);
			nfds = g_iSockID + 1;

			switch(sl_Select(nfds, &readsds, NULL, NULL, &timeout))
			{
			case 0:
			case -1:
				UART_PRINT("Select failed in getting header\r\n");
				goto end;
			default:
				if (SL_FD_ISSET(g_iSockID, &readsds))
					transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);
			}

			if(transfer_len == 0)
			{
				pBuff = NULL;
				UART_PRINT("Header recv end\r\n");
				goto end;
			}
			pBuff = strstr((const char *)g_buff, HTTP_END_OF_HEADER);			//XXX this may cause /r/n in two buff
		}

		// "\r\n\r\n"
		pBuff += 4;

		// Adjust buffer data length for header size
		transfer_len -= (pBuff - g_buff);
	}
	else
	{
		UART_PRINT("Recv header failed\r\n");
		goto end;
	}

	i = 0;
	while (0 < transfer_len)
	{
		i += transfer_len;
		audio_player(pBuff, transfer_len);

		memset(g_buff, 0, sizeof(g_buff));

		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		SL_FD_ZERO(&readsds);
		SL_FD_SET(g_iSockID, &readsds);
		nfds = g_iSockID + 1;

		switch(sl_Select(nfds, &readsds, NULL, NULL, &timeout))
		{
		case 0:
		case -1:
			UART_PRINT("Select failed when playing\r\n");
			goto end;
		default:
			if (SL_FD_ISSET(g_iSockID, &readsds))
			{
				transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);
				pBuff = g_buff;
			}
		}

	}

end:
	audio_play_end();
	sl_Close(g_iSockID);
	return 0;
}

int request_song(char *req, char songs[][128], int max)
{
	int           transfer_len = 0;
	long          retVal = 0;

	char *pBuff = 0;
	char *end = 0;
	char g_buff[MAX_BUFF_SIZE] = {0};
	char host[64] = {0};

	int g_iSockID;
	unsigned long ip;

	int i;

	int nfds;
	SlFdSet_t readsds;
	struct SlTimeval_t timeout;

	pBuff = strstr(req, "http://");
	if (pBuff)
	{
		pBuff = req + 7;
	}
	else
	{
		pBuff = req;
	}

	end = strstr(pBuff, "/");

	if (!end)
	{
		UART_PRINT("Cannot get host name \r\n");
		return -1;
	}

	for(i = 0; i + pBuff < end; i++ )
	{
		host[i] = *(pBuff + i);
	}

	host[i+1] = '\0';
	pBuff = end;

	retVal = sl_NetAppDnsGetHostByName((signed char *)host, strlen(host), &ip, SL_AF_INET);

    if(retVal < 0)
    {
    	UART_PRINT("Cannot get host %s: %x > %d\r\n", host, ip, retVal);
    	return -1;
    }

	g_iSockID = CreateConnection(ip);
	if(g_iSockID < 0)
	{
		UART_PRINT("[%s]open socket failed.\r\n", "request_song");
		return -1;
	}
	memset(g_buff, 0, sizeof(g_buff));

	strcat(g_buff, "GET ");
	strcat(g_buff, pBuff);
	strcat(g_buff, POST_BUFFER_OLD);
	strcat(g_buff, "Host: ");
	strcat(g_buff, host);
	strcat(g_buff, "\r\n");
	strcat(g_buff, HTTP_END_OF_HEADER);

    transfer_len = sl_Send(g_iSockID, g_buff, strlen((const char *)g_buff), 0);

	if (transfer_len < 0)
	{
		UART_PRINT("Socket Send Error\r\n");
		return -1;
	}

	memset(g_buff, 0, sizeof(g_buff));

	// get the reply from the server in buffer.
	transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);

	if(transfer_len > 0)
	{
		// Check for 404 return code
		if(strstr((const char *)g_buff, HTTP_FILE_NOT_FOUND) != 0)
		{
			UART_PRINT("[HTTP] 404\r\n");
			goto end;
		}

		// if not "200 OK" return error
		if(strstr((const char *)g_buff, HTTP_STATUS_OK) == 0)
		{
			UART_PRINT("[HTTP] no 200: %s\r\n", g_buff);
			goto end;
		}

		pBuff = strstr((const char *)g_buff, HTTP_END_OF_HEADER);
		while(pBuff == 0)
		{
			memset(g_buff, 0, sizeof(g_buff));
			transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);
			if(transfer_len <= 0)
				goto end;

			pBuff = strstr((const char *)g_buff, HTTP_END_OF_HEADER);			//XXX this may cause /r/n in two buff
		}

		//"\r\n\r\n"
		pBuff += 4;

		// Adjust buffer data length for header size
		transfer_len -= (pBuff - g_buff);
	}

	UART_PRINT("going to get list\r\n");
	while (0 < transfer_len)
	{
		pBuff = strstr((const char *)g_buff, ".mp3");
		if (pBuff == NULL)
		{
			timeout.tv_sec = 3;
			timeout.tv_usec = 0;

			SL_FD_ZERO(&readsds);
			SL_FD_SET(g_iSockID, &readsds);
			nfds = g_iSockID + 1;

			switch(sl_Select(nfds, &readsds, NULL, NULL, &timeout))
			{
			case 0:
			case -1:
				UART_PRINT("Select failed when getting song list\r\n");
				goto end;
			default:
				if (SL_FD_ISSET(g_iSockID, &readsds))
				{
					memset(g_buff, 0, sizeof(g_buff));
					transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);
				}
			}

		}
		else
		{
			//UART_PRINT("request song : %s\r\n", pBuff);
			if(max ==  get_mp3(g_buff, songs, max))
				break;
		}
	}
end:
	sl_Close(g_iSockID);
	return 0;
}
