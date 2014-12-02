/*
 * vs1053b.c
 *
 *  Created on: 2014Äê7ÔÂ2ÈÕ
 *      Author: Administrator
 */
#include <stdlib.h>
#include "../../cc3200_common.h"

#include "vs1053b.h"
#include "vs_spi.h"

// use global FIFO
#define  AUDIO_FIFO_SIZE  (1024*8)
char* AUDIO_FIFO;
unsigned int AUDIO_FIFO_HEAD = 0;
unsigned int AUDIO_FIFO_TAIL = 0;
unsigned int AUDIO_FIFO_FULL = 0;

/*
#define AUDIO_FIFO_PUT(data, len)		do{	\
	int i, space;	\
	space = AUDIO_FIFO_SIZE - (AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL);	\
	for(i = 0; i < space && i < len ; i++, AUDIO_FIFO_HEAD++)	\
	{	\
		AUDIO_FIFO[(AUDIO_FIFO_HEAD) % AUDIO_FIFO_SIZE] = data[i]; \
	}	\
	len = i; \
	if ((0xFFFFFFFF - AUDIO_FIFO_HEAD) < i)	\
	{	\
		AUDIO_FIFO_HEAD = i - (0xFFFFFFFF - AUDIO_FIFO_HEAD) + AUDIO_FIFO_SIZE;	\
		AUDIO_FIFO_TAIL = AUDIO_FIFO_SIZE - (0xFFFFFFFF - AUDIO_FIFO_TAIL);	\
	}	\
	AUDIO_FIFO_FULL = ((AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL) == AUDIO_FIFO_SIZE) ? 1:0;	\
}while(0);

#define AUDIO_FIFO_GET(data, len)		do{	\
	int i = 0;		\
	if(len > (AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL))	\
		len = AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL;	\
	for(i = 0; i < len; i++)	\
		data[i] = AUDIO_FIFO[(AUDIO_FIFO_TAIL + i) % AUDIO_FIFO_SIZE];	\
	AUDIO_FIFO_TAIL += len;	\
	}while(0);


#define AUDIO_FIFO_CLEAR(len)	do{	\
	len = AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL;	\
}while(0);
*/

//----- DEFINES -----
#define DEFAULT_VOLUME                 (40)     //   0 -   100 %
#define DEFAULT_BASSAMP                (5)      //   0 -    15 dB
#define DEFAULT_BASSFREQ               (100)    //  20 -   150 Hz
#define DEFAULT_TREBLEAMP              (0)      //  -8 -     7 dB
#define DEFAULT_TREBLEFREQ             (15000)  //1000 - 15000 Hz

//VS FiFO
#define VS_BUFSIZE                     (32) //42 kBytes

//VS Type

//Audio Format
#define FORMAT_UNKNOWN                 (0)
#define FORMAT_WAV                     (1)
#define FORMAT_MP3                     (2)
#define FORMAT_AAC                     (3)
#define FORMAT_OGG                     (4)
#define FORMAT_WMA                     (5)
#define FORMAT_FLAC                    (6)

//Clock
#define VS_XTAL                        (12288000UL)
//Opcode
const char VS_READ =	0x03;
const char VS_WRITE = 0x02;
//Register
#define VS_MODE                        (0x00)   //Mode control
#define SM_RESET                       (1<< 2)  //Soft Reset
#define SM_CANCEL                      (1<< 3)  //Cancel Decoding
#define SM_STREAM                      (1<< 6)  //Stream Mode
#define SM_SDINEW                      (1<<11)  //VS1002 native SPI modes
#define VS_STATUS                      (0x01)   //Status
#define VS_BASS                        (0x02)   //Built-in bass/treble enhancer
#define VS_CLOCKF                      (0x03)   //Clock freq + multiplier
#define VS1033_SC_MUL_2X               (0x4000)
#define VS1033_SC_MUL_3X               (0x8000)
#define VS1033_SC_MUL_4X               (0xC000)
#define VS1053_SC_MUL_2X               (0x2000)
#define VS1053_SC_MUL_3X               (0x6000)
#define VS1053_SC_MUL_4X               (0xA000)
#define VS_DECODETIME                  (0x04)   //Decode time in seconds
#define VS_AUDATA                      (0x05)   //Misc. audio data
#define VS_WRAM                        (0x06)   //RAM write/read
#define VS_WRAMADDR                    (0x07)   //Base address for RAM write/read
#define VS_HDAT0                       (0x08)   //Stream header data 0
#define VS_HDAT1                       (0x09)   //Stream header data 1
#define VS_AIADDR                      (0x0A)   //Start address of application
#define VS_VOL                         (0x0B)   //Volume control
//RAM Data
#define VS_RAM_ENDFILLBYTE             (0x1E06)  //End fill byte

#define debug_msg			UART_PRINT

#define STEP							8

typedef union
{
  char  b8[2];
  short b16;
} vs_data_t;

int vs_vol=0, vs_sbamp=0, vs_sbfreq=0, vs_stamp=0, vs_stfreq=0;

int AUDIO_FIFO_PUT(char *data, int len)
{	int i, space;
	space = AUDIO_FIFO_SIZE - (AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL);
	for(i = 0; i < space && i < len ; i++, AUDIO_FIFO_HEAD++)
	{
		AUDIO_FIFO[(AUDIO_FIFO_HEAD) % AUDIO_FIFO_SIZE] = data[i];
	}

	if ((0xFFFFFFFF - AUDIO_FIFO_HEAD) < i)
	{
		AUDIO_FIFO_HEAD = i - (0xFFFFFFFF - AUDIO_FIFO_HEAD) + AUDIO_FIFO_SIZE;
		AUDIO_FIFO_TAIL = AUDIO_FIFO_SIZE - (0xFFFFFFFF - AUDIO_FIFO_TAIL);
	}
	AUDIO_FIFO_FULL = ((AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL) == AUDIO_FIFO_SIZE) ? 1:0;

	return i;
}

int AUDIO_FIFO_GET(char *data, int len)
{
	int i = 0;
	if(len > (AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL))
		len = AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL;
	for(i = 0; i < len; i++)
		data[i] = AUDIO_FIFO[(AUDIO_FIFO_TAIL + i) % AUDIO_FIFO_SIZE];
	AUDIO_FIFO_TAIL += len;

	return i;
}

int AUDIO_FIFO_INIT(void)
{
	AUDIO_FIFO = (char *)malloc(AUDIO_FIFO_SIZE * sizeof(char));
	return 0;
}

void delay_m(int m)
{
	osi_Sleep(m);
}

unsigned short vs_read_reg(char addr)
{
	vs_cs(0);
	vs_write_cmd(VS_READ);

	vs_write_cmd(addr);

	vs_write_cmd(0xff);

	vs_write_cmd(0xff);
	vs_cs(1);
	return 0;
}

void vs_write_reg(char addr, short cmd)
{
	vs_data_t ret;

	ret.b16 = cmd;

	vs_cs(0);
	vs_write_cmd(VS_WRITE);

	vs_write_cmd(addr);

	vs_write_cmd(ret.b8[1]);

	vs_write_cmd(ret.b8[0]);
	vs_cs(1);
}

void vs_write_bass(void)
{
  vs_write_reg(VS_BASS, ((vs_stamp&0x0f)<<12)|((vs_stfreq&0x0f)<<8)|((vs_sbamp&0x0f)<<4)|((vs_sbfreq&0x0f)<<0));

  return;
}

void vs_settreblefreq(int freq) //1000 - 15000Hz
{
  freq /= 1000;

  if(freq < 1) //< 1
  {
    freq = 1;
  }
  else if(freq > 15) //> 15
  {
    freq = 15;
  }
  vs_stfreq = freq;
  vs_write_bass();

  return;
}

void vs_settrebleamp(int amp) //-8 - 7dB
{
  if(amp < -8) //< -8
  {
    amp = -8;
  }
  else if(amp > 7) //> 7
  {
    amp = 7;
  }
  vs_stamp = amp;
  vs_write_bass();

  return;
}

void vs_setbassfreq(int freq) //20 - 150Hz
{
  freq /= 10;

  if(freq < 2) //< 2
  {
    freq = 2;
  }
  else if(freq > 15) //> 15
  {
    freq = 15;
  }
  vs_sbfreq = freq;
  vs_write_bass();

  return;
}

void vs_setbassamp(int amp) //0 - 15dB
{
  if(amp < 0) //< 0
  {
    amp = 0;
  }
  else if(amp > 15) //> 15
  {
    amp = 15;
  }
  vs_sbamp = amp;
  vs_write_bass();

  return;
}



void vs_write_volume(int vol)
{
	short v;

	vol = 100 - vol;

	v = vol;
	v <<= 8;
	v |= vol;

	vs_write_reg(VS_VOL, v);

}


void audio_set_volume(int vol) //0 - 100%
{
  if(vol <= 0) //<= 0
  {
	  vs_write_volume(0);
  }
  else if(vol > 100) //> 100
  {
      vs_write_volume(100);
  }
  else //1 - 99
  {
      vs_write_volume(vol);
  }

  return;
}

void audio_volume(int cmd)
{
	if((cmd & 0x1) == 1)
	{
		 vs_vol += STEP;
		audio_set_volume(vs_vol);
		return;
	}

	if((cmd & 0x1) == 0)
	{
		vs_vol -= STEP;
		audio_set_volume(vs_vol);
		return;
	}

}
void audio_sin_test(void)
{
	char sin_start[8] = {0x53, 0xef, 0x6e, 0x44, 0x00, 0x00, 0x00, 0x00};
	char sin_end[8] = {0x45, 0x78, 0x69, 0x74, 0x00, 0x00, 0x00, 0x00};

	  vs_write_reg(VS_MODE, 0x0820);

	  audio_set_volume(50);

	  vs_spi_clk_data();
	  vs_write_data(sin_start, 8);

	  delay_m(20000);

	  vs_write_data(sin_end, 8);

	  delay_m(500);            /* 500 ms delay_m */
}

void audio_soft_reset(void)
{
	vs_spi_clk_cmd();
	vs_write_reg(VS_MODE, 0x800 | 0x04);

	// set clk
	vs_write_reg(VS_CLOCKF, 0x9800);

	// reset the decode time
	vs_write_reg(VS_DECODETIME, 0x0000);
	vs_write_reg(VS_DECODETIME, 0x0000);
}

int audio_reset(void)
{
	unsigned short version_id = 0;

	vs_rst(0);
	delay_m(100);
	vs_rst(1);

	delay_m(100);
	audio_soft_reset();
	delay_m(100);

	vs_write_reg(VS_CLOCKF, 0x6000);

	vs_spi_clk_data();
	return version_id;
}

void audio_init(void)
{
	vs_rst(1);
	delay_m(100);

	vs_spi_open();

	delay_m(200);
	audio_reset();

	delay_m(50);

	AUDIO_FIFO_INIT();
	//audio_sin_test();

	vs_vol=DEFAULT_VOLUME;
	vs_sbamp=DEFAULT_BASSAMP;
	vs_sbfreq=DEFAULT_BASSFREQ;
	vs_stamp=DEFAULT_TREBLEAMP;
	vs_stfreq=DEFAULT_TREBLEFREQ;

		audio_set_volume(vs_vol);
	  vs_setbassfreq(vs_sbfreq);
	  vs_setbassamp(vs_sbamp);
	  vs_settreblefreq(vs_stfreq);
	  vs_settrebleamp(vs_stamp);
}

int audio_play(int len)
{
	char *data;
	int i = 0;

	if (len <= 0)
		return 0;

	data = (char *)malloc(len * sizeof(*data));
	len = AUDIO_FIFO_GET(data, len);

	for(i = 32; i <= len; i += 32)
		vs_write_data(data + (i - 32), 32);
	if((len + 32 - i) > 0)
		vs_write_data(data + i - 32, len + 32 - i);
	//audio_sin_test();

	free(data);
	//Report("play %d\r\n", len);
	return 1;
}

int audio_play_l(char *data, int len)
{

	if (len <= 0)
		return 0;

	vs_write_data(data, len);
	//audio_sin_test();
	//Report("play %d\r\n", len);
	return 1;
}

int audio_play_start(void)
{
	audio_soft_reset();
	audio_set_volume(vs_vol);
	vs_spi_clk_data();

	return 0;
}

int audio_play_end(void)
{
	audio_play(AUDIO_FIFO_HEAD - AUDIO_FIFO_TAIL);
	return 0;
}

int audio_player(char *data, int len)
{
	int i = 0;
	int j = 0;

	i = len;
	while(i > 0)
	{
		i = AUDIO_FIFO_PUT(data + j, i);
		if (AUDIO_FIFO_FULL)
		{
			audio_play(AUDIO_FIFO_SIZE);
		}
		j += i;

		len = len - i;
		i = len;
	}
	return len;

}
