/*
 * vs1053b.h
 *
 *  Created on: 2014Äê7ÔÂ2ÈÕ
 *      Author: Administrator
 */

#ifndef VS1053B_H_
#define VS1053B_H_

void audio_init(void);
int audio_reset(void);

int audio_player(char *data, int len);
int audio_play_end();
int audio_play_start(void);



#endif /* VS1053B_H_ */
