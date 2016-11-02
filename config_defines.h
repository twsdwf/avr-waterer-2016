#ifndef _CONFIG_DEFINES_INCLUDED_
#define _CONFIG_DEFINES_INCLUDED_

#ifdef MY_ROOM
	#define VERSION_TYPE "MY_ROOM"
#else
	#define VERSION_TYPE "BIG_ROOM"
#endif
#define ESP_BUF_SIZE	100
#define ESP_RST_PIN		9

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
#define PCF8574_ADDRESS		0x20
#define CONFIG_MAGIC		0x55
#define CONFIG_VERSION		0x33

#define GCFG_ADDRESS		32

#define MAX_POTS			80
#define EEPROM_DAYINDEX_ADDR ((MAX_POTS) * 2 + 1)
// #define EXPANDERS_START_PAGE		2
#ifdef MY_ROOM
	#define	WD_SIZE_X	3
	#define WD_SIZE_Y	9
	#define MIN_MOVE_TIME_BTW	1000
	#define MIN_MOVE_LOW_TIME		200
#else
	#define	WD_SIZE_X			7
	#define WD_SIZE_Y			10
	#define MIN_MOVE_TIME_BTW	500
	#define MIN_MOVE_LOW_TIME	200
	#define Y_BEGIN_STOPVAL 	400
	#define BIGROOM_STOPVAL		1001
#endif


#define WFC_PAGE					 	1
#define POTS_DATA_PAGE0					(4)


#define POT_NAME_LENGTH		(18)

#define I2C_MEMORY_ADDRESS		0x50
#define I2C_CLOCK_ADDRESS		0x68	//non-configurable

#define SF_ENABLED	0x01
#define SF_WATER_NOW	0x02
#define SF_TIME2WATER	0x04
#define SF_TIME2DRY		0x08

#define MAX_WFS_COUNT	2

#define F_ESP_USING		(1<<1)

#define RAM_CUR_STATE			(8)
#define CUR_STATE_IDLE			(0)
#define CUR_STATE_READ			(1)
#define CUR_STATE_WATER			(2)

#define LAST_CHECK_TS_1		9
#define LAST_CHECK_TS_2		10
#define LAST_CHECK_TS_3		11
#define LAST_CHECK_TS_4		12

#define RAM_POT_STATE_ADDRESS_BEGIN		(13)
#define RAM_POT_STATE_ADDRESS_END		(RAM_POT_STATE_ADDRESS_BEGIN + 4)
#define RAM_WATER_STATS_BEGIN			(RAM_POT_STATE_ADDRESS_END + 1)

#include "messages_en.h"
#include "pins.h"

#define LED_RED		2
#define LED_YELLOW	4
#define LED_BLUE	6
#define LED_GREEN	0

#define VBTN_WATERTEST	1
#define VBTN_WEN		3

#ifdef MY_ROOM
	#define Z_DOWN_POS_STD		85
	#define Z_TOP_POS			180
	#define Z_AXE_AIN_MIN		100
	#define Z_AXE_AIN_MAX		447
	#define Z_AXE_ANGLE_MIN		10
	#define Z_AXE_ANGLE_MAX		180
#else
	#define Z_DOWN_POS_STD	50
	#define Z_TOP_POS		180
#endif
#define Z_DOWN_POS_EXT	100
#define Z_DOWN_POS_MAX	100

#endif
