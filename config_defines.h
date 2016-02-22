#ifndef _CONFIG_DEFINES_INCLUDED_
#define _CONFIG_DEFINES_INCLUDED_

#define MEM_PAGE_SIZE		32
#define MEM_PAGES			2048 /*65K mem*/

/**
PAGE		content
0			config magic, config version,
1			global config
2			expanderConfig1
3			expanderConfig2
...
16			expanderConfigN
17			pot0

 */

#define CONFIG_MAGIC		0x55
#define CONFIG_VERSION		0x33

#define MAX_POTS			80
// #define EXPANDERS_START_PAGE		2

// #define WATER_STORAGE_PAGE				11

#define WFC_PAGE					 	1
#define POTS_DATA_PAGE0					(4)
// #define WATER_PROGRAMS_START_ADDRESS	(POTS_START_ADDRESS + MAX_POTS)		//86
// #define POTS_STAT_START_BLOCK			(WATER_PROGRAMS_START_ADDRESS + MAX_POTS)	//~200
// #define POTS_STAT_DAYS_BY_POT			512
// // #define POTS_DETAIL_STAT_START_BLOCK	(WATER_PROGRAMS_START_ADDRESS + MAX_POTS + MAX_POTS)
// 
// #define ERR_STORAGE_START			(50000UL)
// #define ERR_STORAGE_MAX_BLOCKS		50
// #define SECOND_COPY_START			(65535UL)

// #define BACKUP_COPY_START			(131072UL)
// #define MEM_PROG_TPL_START		230

// #define MEM_PROG_PROGS_START	65

#define POT_NAME_LENGTH		(16)

#define I2C_EXPANDERS_DDR		DDRE
#define I2C_EXPANDERS_EN_PORT	PORTE
#define I2C_EXPANDERS_EN_PIN	(3)

#define I2C_MEMORY_ADDRESS		0x50
#define I2C_CLOCK_ADDRESS		0x68	//non-configurable
// #define I2C_EXPANDER_PCF	0x01		//deprecated, obsolete
// #define I2C_EXPANDER_MSP	0x02
// #define I2C_EXPANDER_MSP_2SAFE	0x04	//obsolete

#define SF_ENABLED	0x01
#define SF_WATER_NOW	0x02
#define SF_TIME2WATER	0x04
#define SF_TIME2DRY		0x08

#define MAX_WFS_COUNT	2

// #define PUMP_HC595_LATCH	(21)
// #define PUMP_HC595_DATA		(22)
// #define PUMP_HC595_CLOCK	(20)

#define ERRLOG_CNT_BYTE_LO	24
#define ERRLOG_CNT_BYTE_HI	25

#define ERRLOG_TAIL_BYTE_1 26
#define ERRLOG_TAIL_BYTE_2 27
#define ERRLOG_TAIL_BYTE_3 28
#define ERRLOG_TAIL_BYTE_4 29

#define LAST_CHECK_TS_1		31
#define LAST_CHECK_TS_2		32
#define LAST_CHECK_TS_3		33
#define LAST_CHECK_TS_4		34

#define DS1307_POT_STATE_ADDRESS_BEGIN		(10)
#define DS1307_POT_STATE_ADDRESS_END		(20)

// #define USE_OLD_WATERING_DRIVER

// #define		USE_1WIRE

#ifdef USE_1WIRE
	#define ONE_WIRE_PIN	(12)
	#define MAX_DS18B20_SENSORS 2
	#define INDOOR_TERMO	1
	#define OUTDOOR_TERMO	0
#endif
#include "messages_en.h"
#endif
