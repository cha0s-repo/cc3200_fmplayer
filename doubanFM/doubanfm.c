/*
 * doubanfm.c
 *
 *  Created on: 2014Äê7ÔÂ15ÈÕ
 *      Author: Administrator
 */
#include "../cc3200_common.h"
#include "../misc/audio_spi/vs1053b.h"

#include <stdio.h>
#include <string.h>

#include "doubanfm.h"
#include "http.h"

#define FM_HOST		"www.douban.com"
#define FM_CHANNEL	"www.douban.com/j/app/radio/channels"

#define FM_SONG		"www.douban.com/j/app/radio/people?app_name=radio_desktop_win&version=100&channel=%s&type=n"
#define FM_LOGIN	"www.douban.com/j/app/login"

#define LIST_SONG	5
static char song_list[LIST_SONG][128];			// server returns 5 songs every request

int fm_get_channel(unsigned char ch_id[])
{
	return 0;
}

int fm_get_songs(char *ch_id)
{
	char buff[256] = {0};

	sprintf(buff, FM_SONG, ch_id);

	request_song(buff, song_list, LIST_SONG);

	return 0;
}

int fm_play_song(char *song_url)
{
	play_song(song_url);
	return 0;
}

int fm_player(void)
{
	char *song;			// It's very tricky douban encoding track name into sequence number, so the url is pretty formated
	int index = 0;

	Report("Douban FM is ready\r\n");

	while(1)
	{
		//fm_get_channel();

		memset(song_list, 0, sizeof(song_list));
		fm_get_songs("1");

		for(index = 0; index < 10; index++)
		{
			song = song_list[index];
			if (strlen(song))
			{
				Report("Going to play : %s\r\n", song);
				 GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
				fm_play_song(song);
				 GPIO_IF_LedOff(MCU_ORANGE_LED_GPIO);
			}
			else
			{
				// wait for next try
				Report("Cannot fitch a song to play, waiting for next try\r\n");
				osi_Sleep(3000);
				break;
			}
		}
	}

}
