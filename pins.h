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

// #define Z_AXE_DIR			(35) free
// #define Z_AXE_EN			(36) free


#define VCC_PUMP_EN			34

#define Y_AXE_EN			39
#define X_AXE_EN			40

#define AIN_PIN				A1 //A1 //analog

#define Y_END_PIN			48 //A5

#define X_BEGIN_PIN			23
#define Y_BEGIN_PIN			52 // digital!
#ifdef MY_ROOM
// 	#define PLANT_LIGHT_PIN 	12
	#define AQUARIUM_PIN		51
	#define Z_SERVO_FEEDBACK_PIN	(A0)
	#define XY_VCC_DELTA		5
	#define Z_AXE_UP_PIN		A6
	#define Z_AXE_DOWN_PIN		35 //digital
#else
	#define Z_AXE_UP_PIN	 	51
#endif
#define Z_AXE_SERVO_PIN		46


#endif