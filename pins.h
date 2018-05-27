#ifndef _PINS_FILE_INC_

#define _PINS_FILE_INC_

#define J16_BROWN	(27)
#define J16_RED		(29)
#define J16_ORANGE	(31)
#define J16_YELLOW	(33)
#define J16_GREEN	(35)
#define J16_BLUE	(36)
#define J16_VIOLET	(34)
#define J16_GRAY	(32)
#define J16_WHITE	(30)
#define J16_BLACK	(28)


#define X_BEGIN_PIN			35
#define Y_BEGIN_PIN			33

#define X_END_PIN			36
#define Y_END_PIN			34

#define X_STEP_PIN			5
#define Y_STEP_PIN			6


#define Z_AXE_EN    28
#define Z_AXE_DIR   27
#define Z_AXE_DIR2  32

#define Z_BEGIN_PIN	 	(30)
#define Z_END_PIN		(29)


#define WFS_PIN				(21)
#define WFS2_PIN			(22)
#define PUMP_PIN			(23)
#define PUMP2_PIN			(22)
#define AIR_PIN				(24)


#define X_AXE_EN			41
#define X_AXE_DIR			39
#define X_AXE_DIR2			40

#define Y_AXE_EN			38
#define Y_AXE_DIR			37
#define Y_AXE_DIR2			26

#define AIN_PIN				A0 //A1 //analog

#define I2C_EXPANDERS_DDR		DDRE
#define I2C_EXPANDERS_EN_PORT	PORTE
#define I2C_EXPANDERS_EN_PIN	(2)	//D2


//OLD BELOW

//D0 & D1 -- Serial
//D2 -- blink LED

// PB 0 **  8 ** SPI_SS
// PB 1 **  9 ** SPI_SCK
// PB 2 ** 10 ** SPI_MOSI
// PB 3 ** 11 ** SPI_MISO

// D16 & D17 -- IIC
// D18 & D19 --Serial1

#ifdef MY_ROOM
// 	#define PLANT_LIGHT_PIN 	12
	//#define AQUARIUM_PIN		51
// 	#define Z_AXE_UP_PIN		A6
// 	#define Z_AXE_DOWN_PIN		35 //digital
#else
// 	#define Z_AXE_UP_PIN	 	51
#endif



#endif