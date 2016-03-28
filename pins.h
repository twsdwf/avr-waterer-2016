#ifndef _PINS_FILE_INC_

#define _PINS_FILE_INC_


//D0 & D1 -- Serial
//D2 -- blink LED
#define I2C_EXPANDERS_DDR		DDRE
#define I2C_EXPANDERS_EN_PORT	PORTE
#define I2C_EXPANDERS_EN_PIN	(3)	//D3

// PB 0 **  8 ** SPI_SS
// PB 1 **  9 ** SPI_SCK
// PB 2 ** 10 ** SPI_MOSI
// PB 3 ** 11 ** SPI_MISO

// D16 & D17 -- IIC
// D18 & D19 --Serial1

#define WFS_PIN				(20)
#define PUMP_PIN			(21)
#define AIR_PIN				(22)
//24 25 -- free
#define X_AXE_DIR			26
#define X_AXE_DIR2			27

#define Y_AXE_DIR			28
#define Y_AXE_DIR2			29

#define X_STEP_PIN			(31)
#define Y_STEP_PIN			(30)

#define Z_AXE_DIR			(35)
#define Z_AXE_EN			(36)


#define VCC_PUMP_EN			34

#define Y_AXE_EN			39
#define X_AXE_EN			40

#define AIN_PIN				44 //A1 //analog

#define Y_END_PIN			48 //A5

#ifdef MY_ROOM
	#define PLANT_LIGHT_PIN 	12
	#define X_BEGIN_PIN			23
	#define AQUARIUM_PIN		51
	#define Y_BEGIN_PIN			52 // digital!
#else
	#define X_BEGIN_PIN			47// A4
	#define Y_BEGIN_PIN			(50) //(A7)
#endif


#endif