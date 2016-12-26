#ifndef _ARDUWATER_TYPES_H_
#define _ARDUWATER_TYPES_H_

#include "Arduino.h"
#include "config_defines.h"

#undef PROGMEM

#define PROGMEM __attribute__((section(".progmem.data")))

#ifndef EXTRACT_STRING
#define EXTRACT_STRING(dst, src) strcpy_P(dst, (char*)(src));
#endif

/*
	надо ввести "сухостойкость" растений. число 0-3, которое описывает приоритет полива, если известно, что вода для полива на исходе.
	соотв, настройка Н, которое описывает две границы остатка воды для полива: "жёлтая", когда перестают поливаться растения
	минимального приоритета и "красная", когда поливаются только "высокоприоритетные" растения.

	шаблоны программ:
		1. постоянная влажность по планке А(не поливать вообще, если некоторое "волшебное число" как планка).
		2. полив до планки А, потом подсушка до планки Б, потом снова полив
		3. полив Хмл раз в Ч часов если влажность меньше планки А
		4. программа полива А, но с часа Х до часа У(интервал внутри глобального интервала полива).
		5. программа А летом и программа Б зимой
		6. программа А пока не менее Х градусов и программа Б иначе
	программы -- набор структур с номером шаблона и его параметрами. не более 16 программ.
	параметры -- массив из 3х 32битных чисел.
 */

typedef struct wpConstantHum{//1
	uint16_t 	value;
	uint16_t 	max_ml;
}wpConstantHum;//4

typedef struct wpHumAndDry{//2
	uint16_t 	min_value;
	uint16_t 	max_value;
	uint16_t 	max_ml;
}wpHumAndDry;//6

typedef struct wpByTime{//3
	uint16_t min_value;
	uint16_t interval;
	uint8_t ml;
}wpByTime;//size = 5


typedef union atomicProgram{
	wpConstantHum 	const_hum;
	wpHumAndDry		hum_and_dry;
	wpByTime		by_time;
}atomicProgram;

typedef struct wpShrinkedTime{//4
	uint16_t from_time;
	uint16_t to_time;
	atomicProgram program;
}wpShrinkedTime;//size=5  d=+4

typedef struct wpSumAndWin{//5
	uint16_t sum_from;//time
	uint16_t win_from;//time
	atomicProgram sum_prog;
	atomicProgram win_prog;
}wpSumAndWin;//size=6, d=+4

typedef struct wpTempBounds{
	uint8_t temp_barrier;
	atomicProgram program_below;
	atomicProgram program_above;
}wpTempBounds;//size=3, d=+1


typedef struct wateringProgram{
	union{
		atomicProgram		prog;
		wpShrinkedTime		shrinked_time;
		wpSumAndWin			sum_and_win;
		wpTempBounds		temp_bounds;
	};
}wateringProgram;

typedef struct sensorValues{
	uint8_t address;
	int pin_values[16];
}sensorValues;

#define WCFG_STATE_WATERING		0x00
#define WCFG_STATE_DRYING		0x02

// #define WCF_EN		0x01
// #define WCF_SET_EN(x, state) {(x).wc.flags &= 0xFE;(x).wc.flags |= (state?WCF_EN:0);}
// #define WCF_GET_EN(x) ((x).wc.flags & WCF_EN)
// #define WCF_GET_PROGRAM_ID(x)	(( (x).wc.flags>>4) & 0x0F)
// #define WCF_SET_PROGRAM_ID(x, pid)	{(x).wc.flags = ((x).wc.flags &0x0F) | ((pid)<<4);}
// #define WCF_GET_AIRTIME(x) ( ((x).wc.flags &0x0A)>>2)
// #define WCF_SET_AIRTIME(x, val) {(x).wc.flags &= 0x0A; (x).wc.flags |= ((uint8_t)val & 0x03) << 2;}
typedef struct coords{
	uint8_t	x:3;
	uint8_t y:5;
	uint8_t index;
}coords;

typedef struct wateringConfig{
	uint8_t	x:3;
	uint8_t y:5;
	uint8_t pgm:4;
	uint8_t airTime:2;
	uint8_t state:1;
	uint8_t enabled:1;
//  	uint8_t flags;
	uint8_t	ml;
	uint16_t watered;
}wateringConfig;//6 bytes

typedef struct sensorConfig
{
	uint8_t dev;
 	uint8_t pin;
// 	uint8_t flags;
	uint8_t noise_delta;
} sensorConfig; //4

typedef struct potConfig{
	sensorConfig	sensor;			//6
	wateringConfig wc;				//4
	atomicProgram pgm;				//6
	char name[POT_NAME_LENGTH];		//32-23-1
}potConfig;

inline int IS_P(char*cmd, const char*pgm_str, uint8_t n)
{
	return 0==strncmp_P(cmd, pgm_str, n);
}

inline int IS(char*cmd, char*x, uint8_t n)
{
	return !strncmp(cmd, x, n);
}

inline int IS(char*cmd, char*x)
{
	return !strncmp(cmd, x, strlen(cmd));
}

/*
typedef struct expanderConfig
{
	uint8_t	address;
	uint8_t type;
	uint8_t flags;
} expanderConfig;*/


typedef struct WaterDoserSystemConfig{
	uint8_t flags;
	uint8_t pump_pin; // only 1..5 !!!
	uint8_t pump2_pin; // only 1..5 !!!
	uint8_t wfs_pin; // only 4..7 !!!
	uint8_t wfs2_pin; // only 4..7 !!!
	uint16_t wfs_ticks_per_liter;
	uint16_t wfs2_ticks_per_liter;
	uint8_t doser_move_pin;
	uint8_t doser_dir_pin;
	uint8_t servo_pin;
	uint8_t servo2_pin;
	uint8_t optocoupler_pin;
	uint8_t end_switch_pin;
} WaterDoserSystemConfig;//11bytes


#define GCFG_SENSOR_UNIT_PARROTS	0x40

typedef struct globalConfig{
	uint16_t enabled:1;
	uint16_t esp_en:1;
	uint16_t flags:14;
// 	uint8_t i2c_expanders_count;
	uint8_t pots_count;
// 	uint8_t dosers_count;
	uint16_t i2c_pwron_timeout;
	uint16_t sensor_init_time;
	uint16_t sensor_read_time;
	uint16_t water_start_time;
	uint16_t water_end_time;
	uint16_t water_start_time_we;
	uint16_t water_end_time_we;
	uint8_t sensor_measures;
	uint8_t test_interval;
 	uint16_t lux_barrier_value;
	/*water doser Z-axe servo angles (top default, top dead center, down dead center*/
	uint8_t wdz_top;
	uint8_t wdz_tdc;
	uint8_t wdz_ddc;
// 	uint8_t ws_count; future use
}globalConfig;//21 bytes

extern HardwareSerial Serial1;
// extern Configuration g_cfg;



inline void get_h_m_from_timefield(uint16_t& time, uint8_t&h, uint8_t&m)
{
	h = (time >> 8) & 0xFF;
	m = time & 0xFF;
}


template<typename field_type> void print_field(HardwareSerial* output, field_type field, char terminator=',')
{
	output->print(field);
	output->print(terminator);
// 	Serial1.print(field);
// 	Serial1.print(terminator);
}

template<typename field_type> bool set_field(field_type& field, char**ptr)
{
	field_type val = 0;
	bool ok = false;
	while(**ptr && isdigit(**ptr)) {
		ok = true;
		val *= 10;
		val += **ptr -'0';
		++(*ptr);
	}
	if (ok) {
//  		Serial1.print("field is ok.value=");
// 		Serial1.println(val);
		field = val;
	} else {
//  		Serial1.println("field read error");
	}
	++*ptr;
	return ok;
}

typedef struct DayWaterEntry{
	uint16_t time;
	uint32_t sensor_value;
	uint16_t ml;
}DayWaterEntry;

typedef struct DayWaterSector{
	uint32_t date;
	int8_t last_record;
	DayWaterEntry entries[63];
	uint8_t dummy[3];
}DayWaterSector;


typedef struct WaterStorageData{
 	uint8_t flags:3;
	uint8_t prior:4;
	uint8_t enabled:1;
	uint16_t vol;
	uint16_t spent;
	uint8_t pump_pin;
	char name[17];
}WaterStorageData;
#endif

