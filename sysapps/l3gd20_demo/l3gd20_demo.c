/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */
#include "l3gd20.h"
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define ABS(x)                     (x < 0) ? (-x) : x
#define L3G_Sensitivity_250dps     (float)114.285f        /*!< gyroscope sensitivity with 250 dps full scale [LSB/dps]  */
#define L3G_Sensitivity_500dps     (float)57.1429f        /*!< gyroscope sensitivity with 500 dps full scale [LSB/dps]  */
#define L3G_Sensitivity_2000dps    (float)14.285f         /*!< gyroscope sensitivity with 2000 dps full scale [LSB/dps] */
  

/* L3GD20 struct */
typedef struct 
{
  uint8_t power_mode;                         /* Power-down/Sleep/Normal Mode */
  uint8_t datarate;                    	   /* OUT data rate */
  uint8_t axes_enable;                        /* Axes enable */
  uint8_t bandwidth;                         /* Bandwidth selection */
  uint8_t blockdata_update;                   /* Block Data Update */
  uint8_t endianness;                         /* Endian Data selection */
  uint8_t scale;                         /* Full Scale selection */
}l3gd20_init_t;

/* L3GD20 High Pass Filter struct */
typedef struct
{
  uint8_t mode;      /* Internal filter mode */
  uint8_t cutoff_freq;    /* High pass filter cut-off frequency */
}l3gd20_filter_config_t;


static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spi0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;
static float Gyro[3];

static int 
check_device(int fd)
{
	int ret;
	uint8_t tx[] = {
		(L3GD20_WHO_AM_I_ADDR | READWRITE_CMD ),0xFF,
	};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	if (rx[1] == I_AM_L3GD20) 
		return 0;
	else 
		return 1;
}

static int 
init_l3gd20(int fd, l3gd20_init_t *init)
{
	int ret;
	uint8_t ctrl1 = 0x00, ctrl4 = 0x00;
	
	/* Configure MEMS: data rate, power mode, full scale and axes */

	ctrl1 |= (uint8_t) (init->power_mode | init->datarate | \
                    init->axes_enable | init->bandwidth);
  
  	ctrl4 |= (uint8_t) (init->blockdata_update | init->endianness | \
                    init->scale);

	uint8_t tx[] = {
		L3GD20_CTRL_REG1_ADDR,ctrl1,
	};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	for (ret = 0; ret < ARRAY_SIZE(tx); ret++) {
		if (!(ret % 6))
			puts("");
		printf("%.2X ", rx[ret]);
	}
	puts("");

	tx[0] = L3GD20_CTRL_REG4_ADDR; tx[1] = ctrl4;
	rx[0] = 0; rx[1] = 0;
	
 	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");
	
	return 0;
}


static int 
config_l3gd20(int fd, l3gd20_filter_config_t *config)
{
	int ret;
	uint8_t tmpreg;

 	uint8_t tx[] = {
		(L3GD20_CTRL_REG2_ADDR | READWRITE_CMD ),0xFF,
	};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	/* Read CTRL_REG2 register */
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	rx[1] &= 0xC0;

	/* Configure MEMS: mode and cutoff frquency */
  	tmpreg |= (uint8_t) (config->mode | config->cutoff_freq);
                          
  	/* Write value to MEMS CTRL_REG2 regsister */
	tx[0] = L3GD20_CTRL_REG2_ADDR; tx[1] = tmpreg;
	rx[0] = 0; rx[1] = 0;
	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	return 0;
}

static int
read_l3gd20(int fd, float* pfData)
{
	int ret;
	int16_t RawData[3] = {0};
	uint8_t tmpreg = 0;
	float sensitivity = 0;
	int i =0;

	uint8_t tx[] = {
		(L3GD20_CTRL_REG4_ADDR | READWRITE_CMD ),0xFF,
	};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };

	uint8_t out_tx[6] = {
		(L3GD20_OUT_X_L_ADDR | READWRITE_CMD ),0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint8_t out_rx[6] = {0, };

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	/* Read CTRL_REG4 register */
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");
  	
	tmpreg = rx[1];

	struct spi_ioc_transfer tr_out = {
		.tx_buf = (unsigned long)out_tx,
		.rx_buf = (unsigned long)out_rx,
		.len = 6,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	/* Read L3GD20_OUT_X_L_ADDR register */
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr_out);
	if (ret < 1)
		pabort("can't send spi message");
  	
  
	/* check in the control register 4 the data alignment (Big Endian or Little Endian)*/
	if(!(tmpreg & 0x40)) {
		for(i=0; i<3; i++){
			RawData[i]=(int16_t)(((uint16_t)out_rx[2*i+1] << 8) + out_rx[2*i]);
		}
	}
	else {
		for(i=0; i<3; i++){
			RawData[i]=(int16_t)(((uint16_t)out_rx[2*i] << 8) + out_rx[2*i+1]);
		}
	}
  
	/* Switch the sensitivity value set in the CRTL4 */
	switch(tmpreg & 0x30) 	{
		case 0x00:
		sensitivity = L3G_Sensitivity_250dps;
		break;

		case 0x10:
		sensitivity=L3G_Sensitivity_500dps;
		break;

		case 0x20:
		sensitivity=L3G_Sensitivity_2000dps;
		break;
	}
	/* divide by sensitivity */
	for(i=0; i<3; i++){
		pfData[i]=(float)RawData[i]/sensitivity;
	}
}


static void print_usage(const char *prog)
{
	printf("Usage: %s [-DsbdlHOLC3]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word \n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n");
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:lHOLC3NR", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'L':
			mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			mode |= SPI_CS_HIGH;
			break;
		case '3':
			mode |= SPI_3WIRE;
			break;
		case 'N':
			mode |= SPI_NO_CS;
			break;
		case 'R':
			mode |= SPI_READY;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;

	parse_opts(argc, argv);

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	printf("say hello to l3gd20\n");

	if (check_device(fd))
		printf("no l3gd20 there\n");
	else
		printf("Got DeviceID\n");


	l3gd20_init_t 	l3gd20_init;
	/* Configure Mems L3GD20 */
	l3gd20_init.power_mode = L3GD20_MODE_ACTIVE;
	l3gd20_init.datarate = L3GD20_OUTPUT_DATARATE_1;
	l3gd20_init.axes_enable = L3GD20_AXES_ENABLE;
	l3gd20_init.bandwidth = L3GD20_BANDWIDTH_4;
	l3gd20_init.blockdata_update = L3GD20_BlockDataUpdate_Continous;
	l3gd20_init.endianness = L3GD20_BLE_LSB;
	l3gd20_init.scale = L3GD20_FULLSCALE_500; 

	init_l3gd20(fd, &l3gd20_init);

	l3gd20_filter_config_t config;
	config.mode = L3GD20_HPM_NORMAL_MODE_RES;
	config.cutoff_freq = L3GD20_HPFCF_0;	
	config_l3gd20(fd, &config);
	
	while (1) {
		read_l3gd20(fd, Gyro);
		printf ("Gyro X: %f Y: %f Z: %f \n", Gyro[0], Gyro[1], Gyro[2]);
		usleep(50000);
	}

	close(fd);

	return ret;
}

