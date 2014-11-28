/*
 * spi.h
 *
 *  Created on: 2014��7��8��
 *      Author: Administrator
 */

#ifndef SPI_H_
#define SPI_H_

int vs_write_cmd(char c);
int vs_write_data(char *d, int len);

void vs_cs(char cmd);
void vs_dcs(char cmd);
void vs_rst(char cmd);

void vs_spi_open(void);

int vs_req(void);

void vs_spi_clk_cmd();
void vs_spi_clk_data();

#endif /* SPI_H_ */
