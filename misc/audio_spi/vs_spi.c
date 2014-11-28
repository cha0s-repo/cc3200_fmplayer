/*
 * spi.c
 *
 *  Created on: 2014Äê7ÔÂ8ÈÕ
 *      Author: Administrator
 */
#include "../../cc3200_common.h"
#include "gpio.h"
#include "spi.h"

#include <string.h>

#include "vs_spi.h"

#define ASSERT_CS()         GPIOPinWrite(GPIOA3_BASE, 0x40, 0x00)
#define DEASSERT_CS()       GPIOPinWrite(GPIOA3_BASE, 0x40, 1 << (30 % 8))

#define ASSERT_DCS()         GPIOPinWrite(GPIOA3_BASE, 0x80, 0x00)
#define DEASSERT_DCS()       GPIOPinWrite(GPIOA3_BASE, 0x80, 1 << (31 % 8))


#define MASTER_MODE      0

#define VS_CLK			12288000
#define VS_CLK_CMD			(VS_CLK/7)
#define VS_CLK_DATA			(VS_CLK/4)

#define SPI_IF_BIT_RATE  125000

static char g_ucTxBuff[32];
static char g_ucRxBuff[32];

void vs_spi_clk_cmd(void)
{
	MAP_SPIDisable(GSPI_BASE);
	  MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
	                     VS_CLK_CMD,SPI_MODE_MASTER,SPI_SUB_MODE_0,
	                     (SPI_SW_CTRL_CS |
	                     SPI_4PIN_MODE |
	                     SPI_TURBO_OFF |
	                     SPI_CS_ACTIVEHIGH |
	                     SPI_WL_8));
	  MAP_SPIEnable(GSPI_BASE);
}

void vs_spi_clk_data(void)
{
		MAP_SPIDisable(GSPI_BASE);
	  MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
	                     VS_CLK_DATA,SPI_MODE_MASTER,SPI_SUB_MODE_0,
	                     (SPI_SW_CTRL_CS |
	                     SPI_4PIN_MODE |
	                     SPI_TURBO_OFF |
	                     SPI_CS_ACTIVEHIGH |
	                     SPI_WL_8));
	  MAP_SPIEnable(GSPI_BASE);
}
void vs_spi_open(void)
{
	  //
	  // Reset SPI
	  //
	  MAP_SPIReset(GSPI_BASE);

	  //
	  // Configure SPI interface
	  //
	  MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
	                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
	                     (SPI_SW_CTRL_CS |
	                     SPI_4PIN_MODE |
	                     SPI_TURBO_OFF |
	                     SPI_CS_ACTIVEHIGH |
	                     SPI_WL_8));

	  //
	  // Enable SPI for communication
	  //
	  MAP_SPIEnable(GSPI_BASE);

	  DEASSERT_CS();
	  DEASSERT_DCS();
}


int vs_req(void)
{
	return (GPIOPinRead(GPIOA3_BASE, 0x10) >> (28 %8));
}

void vs_rst(char cmd)
{
	if (cmd)
		GPIOPinWrite(GPIOA0_BASE, 0x40, 1 << (6 % 8));
	else
		GPIOPinWrite(GPIOA0_BASE, 0x40, 0x00);
}


void vs_cs(char cmd)
{
	if (cmd)
		GPIOPinWrite(GPIOA3_BASE, 0x40, 1 << (30 % 8));
	else
		GPIOPinWrite(GPIOA3_BASE, 0x40, 0x00);
}

void vs_dcs(char cmd)
{
	if (cmd)
		GPIOPinWrite(GPIOA3_BASE, 0x80, 1 << (31 % 8));
	else
		GPIOPinWrite(GPIOA3_BASE, 0x80, 0x00);
}

int vs_write_cmd(char c)
{
	memset(g_ucTxBuff, '\0', 32);
	g_ucTxBuff[0] = c;

	while(!vs_req());

    MAP_SPITransfer(GSPI_BASE,(unsigned char *)g_ucTxBuff,(unsigned char *)g_ucRxBuff,1,
                SPI_CS_ENABLE|SPI_CS_DISABLE);

	return 0;
}

int vs_write_data(char *d, int len)
{
	memset(g_ucTxBuff, '\0', 32);
	memcpy(g_ucTxBuff, d, len);

	while(!vs_req());

	ASSERT_DCS();
    MAP_SPITransfer(GSPI_BASE,(unsigned char *)g_ucTxBuff,(unsigned char *)g_ucRxBuff,len,
                SPI_CS_ENABLE|SPI_CS_DISABLE);
    DEASSERT_DCS();

	return 0;
}
