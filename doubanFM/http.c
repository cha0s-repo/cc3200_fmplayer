/*
 * http.c
 *
 *  Created on: 2014��6��24��
 *      Author: Administrator
 */
#include "../cc3200_common.h"

#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "../misc/audio_spi/vs1053b.h"

#define UARTprintf	Report
#define UART_PRINT	Report

#define POST_BUFFER_OLD     " HTTP/1.1 \r\nAccept: application/xml,application/xhtml+xml,text/html,*/*;q=0.5\r\nConnection:keep-alive\r\nUser-Agent:Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US)\r\n"
#define HTTP_FILE_NOT_FOUND    "404 Not Found" /* HTTP file not found response */
#define HTTP_STATUS_OK         "200 OK"  /* HTTP status ok response */
#define HTTP_CONTENT_LENGTH    "Content-Length:"  /* HTTP content length header */
#define HTTP_CONTENT_TYPE		"Content-type:"
#define HTTP_TRANSFER_ENCODING "Transfer-Encoding:" /* HTTP transfer encoding header */
#define HTTP_ENCODING_CHUNKED  "chunked" /* HTTP transfer encoding header value */
#define HTTP_CONNECTION        "Connection:" /* HTTP Connection header */
#define HTTP_CONNECTION_CLOSE  "close"  /* HTTP Connection header value */

#define DNS_RETRY       5 /* No of DNS tries */

#define HTTP_END_OF_HEADER  "\r\n\r\n"  /* string marking the end of headers in response */

#define MAX_BUFF_SIZE  1460



enum
{
    CONNECTED = 0x1,
    IP_ACQUIRED = 0x2
}e_Stauts;


void CLI_Write(const char *msg)
{
	UARTprintf(msg);
}

int CreateConnection(unsigned long DestinationIP)
{
    SlSockAddrIn_t  Addr;
    int             Status = 0;
    int             AddrSize = 0;
    int             SockID = 0;
    SlSockNonblocking_t enableOption;

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(80);
    Addr.sin_addr.s_addr = sl_Htonl(DestinationIP);

    AddrSize = sizeof(SlSockAddrIn_t);

    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( SockID < 0 )
    {
        /* Error */
        CLI_Write("Error while opening the socket\r\n");
        return -1;
    }

    enableOption.NonblockingEnabled = 1;
    sl_SetSockOpt(SockID,SOL_SOCKET,SL_SO_NONBLOCKING, &enableOption,sizeof(enableOption)); // Enable/disable nonblocking mode
    Status = sl_Connect(SockID, ( SlSockAddr_t *)&Addr, AddrSize);
    if( Status < 0 )
    {
        /* Error */
        UART_PRINT("Error during connection with server: %d\r\n", Status);
        sl_Close(SockID);
        return -1;
    }
    return SockID;
}

unsigned long hexToi(unsigned char *ptr)
{
    unsigned long result = 0;
    int idx = 0;
    unsigned int len = strlen((const char *)ptr);

    /* convert charecters to upper case */
    for(idx = 0; ptr[idx] != '\0'; ++idx)
        if(ptr[idx] > 96 && ptr[idx] < 103)
            ptr[idx] -= 32;

    for(idx = 0; ptr[idx] != '\0'; ++idx)
    {
        if(ptr[idx] <= 57)
            result += (ptr[idx]-48)*(1<<(4*(len-1-idx)));
        else
            result += (ptr[idx]-55)*(1<<(4*(len-1-idx)));
    }

    return result;
}

//****************************************************************************
//
//! \brief Obtain the file from the server
//!
//! This function requests the file from the server and save it in
//! temporary file (TEMP_FILE_NAME) in serial flash.
//! To request a different file for different user needs to modify the
//! PREFIX_BUFFER and POST_BUFFER macros.
//!
//! \param[in]      None
//!
//! \return         0 for success and negative for error
//
//****************************************************************************
int play_song(char *req)
{
	int           transfer_len = 0;
	long          retVal = 0;

	char *pBuff = 0;
	char *end = 0;

//	unsigned int bytesReceived;

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
		Report("Cannot get host name \r\n");
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
    	Report("Cannot get host %s: %d\r\n", host, ip);
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
/*
		// if not "200 OK" return error
		if(strstr((const char *)g_buff, HTTP_STATUS_OK) == 0)
		{
			UART_PRINT("[HTTP] no 200; %d\r\n", transfer_len);

			goto prep;
		}
*/
		// "\r\n\r\n" marks the end of headers

		//pBuff = strstr((const char *)g_buff, HTTP_END_OF_HEADER);
		pBuff = g_buff;
		while(pBuff != NULL)											// XXX
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

			//pBuff = strstr((const char *)g_buff, HTTP_END_OF_HEADER);			//XXX this may cause /r/n in two buff
			if(transfer_len == 0)
				pBuff = NULL;
		}
		UART_PRINT("Recv song data done \r\n");
//		goto end;
		// Increment by 4 to skip "\r\n\r\n"
		pBuff += 4;

		// Adjust buffer data length for header size
		transfer_len -= (pBuff - g_buff);
	}
	else
		goto end;

	while (0 < transfer_len)
	{
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

int get_mp3(char *buff, char songs[][128], int max)
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
		Report("Cannot get host name \r\n");
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
    	Report("Cannot get host %s: %x > %d\r\n", host, ip, retVal);
    	return -1;
    }


	g_iSockID = CreateConnection(ip);
	if(g_iSockID < 0)
	{
		UART_PRINT("open socket failed\r\n");
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

/*
	sprintf(g_buff, POST_BUFFER, pBuff, host);
	strcat(g_buff, HTTP_END_OF_HEADER);
*/
//	UART_PRINT("SEND: %s\r\n", g_buff);
	// Send the HTTP GET string to the opened TCP/IP socket.
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

		// check if content length is transfered with headers
		pBuff = strstr((const char *)g_buff, HTTP_CONTENT_LENGTH);
		if(pBuff != 0)
		{
			// not supported
			//UART_PRINT("Server response format is not supported\r\n");//youmaywant read g_buff
			//UART_PRINT(g_buff);
			//return -1;
		}

		// "\r\n\r\n" marks the end of headers

		pBuff = (char *)strstr((const char *)g_buff, HTTP_END_OF_HEADER);
		while(pBuff == 0)
		{
			memset(g_buff, 0, sizeof(g_buff));
			transfer_len = sl_Recv(g_iSockID, &g_buff[0], MAX_BUFF_SIZE, 0);
			pBuff = (char *)strstr((const char *)g_buff, HTTP_END_OF_HEADER);			//XXX this may cause /r/n in two buff
		}

		// Increment by 4 to skip "\r\n\r\n"
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
			//UART_PRINT("recv......\r\n");
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
			//Report("request song : %s\r\n", pBuff);

			if(max ==  get_mp3(g_buff, songs, max))
				break;
		}
	}
	UART_PRINT("get list done\r\n");
end:
	sl_Close(g_iSockID);
	return 0;
}
