
#include "Arduino.h"

#include "awtypes.h"
#include "config_defines.h"
// #include "mmc.h"
#include "configuration.h"

#include "RTClib.h"

extern RTC_DS1307 clock;
// static char wrong_magic[] PROGMEM = "bad magic!";

Configuration::Configuration()
{
	memset(&this->gcfg, 0, sizeof(gcfg));
}

Configuration::~Configuration()
{

}
/*
void Configuration::setMemoryDevice()
{
// 	this->mem = _mem;
}*/

void Configuration::begin()
{
	pinMode(8, OUTPUT);//SS
// 	uint32_t t = millis();
//  	mmc::initialize(SPISPEED_HIGH);
// 	Serial1.print("init mmc:");
// 	Serial1.println(millis() - t);
// 	t = millis();
//  	this->readGlobalConfig();
// 	Serial1.print("read config:");
// 	Serial1.println(millis() - t);
}

bool Configuration::readWaterStorageData(WaterStorageData*wsd, uint8_t index)
{
	uint8_t buffer[512] = {0};
// 	mmc::readSector(buffer, WATER_STORAGE_PAGE);
// 	for (int i =0 ;i < 512;++i) {
// 		Serial1.print(buffer[i], DEC);
// 		if (i && (i %16 == 0))
// 			Serial1.println();
// 		else
// 			Serial1.print(" ");
// 	}
// 	Serial1.println("\n----\n");
// 	memcpy(wsd, buffer + 2 + index * sizeof(WaterStorageData), sizeof(WaterStorageData));
// 			Serial1.print("cfg rd storage:");
// 			Serial1.println(index, DEC);
// 			Serial1.print(" ");
// 			Serial1.print(wsd->name);
// 			Serial1.print(" ");
// 			Serial1.print(wsd->vol, DEC);
// 			Serial1.print(" ");
// 			Serial1.println(wsd->spent, DEC);
// 	Serial1.print("read wdsc config:");
// 	Serial1.println(millis() - t);
	return true;
}
uint8_t Configuration::readWScount()
{
	uint8_t buffer[512] = {0};
// 	mmc::readSector(buffer, WATER_STORAGE_PAGE);
	return buffer[0];
}

void Configuration::setWScount(uint8_t count)
{
	uint8_t buffer[512] = {0};
// 	mmc::readSector(buffer, WATER_STORAGE_PAGE);
// 	buffer[0] = count;
// 	buffer[1] = count;
// 	mmc::writeSector(buffer, WATER_STORAGE_PAGE);
// 	mmc::writeSector(buffer, SECOND_COPY_START + WATER_STORAGE_PAGE);
}

bool Configuration::writeWaterStorageData(WaterStorageData*wsd, uint8_t index)
{
	uint8_t buffer[512] = {0};
// 			Serial1.print("cfg wr storage:");
// 			Serial1.println(index, DEC);
// 			Serial1.print(" ");
// 			Serial1.print(wsd->name);
// 			Serial1.print(" ");
// 			Serial1.print(wsd->vol, DEC);
// 			Serial1.print(" ");
// 			Serial1.println(wsd->spent, DEC);
// 	mmc::readSector(buffer, WATER_STORAGE_PAGE);
// 	memcpy(buffer + 2 + index * sizeof(WaterStorageData), wsd, sizeof(WaterStorageData));
// 	for (int i =0 ;i < 512;++i) {
// 		Serial1.print(buffer[i], DEC);
// 		if (i && (i %16 == 0))
// 			Serial1.println();
// 		else
// 			Serial1.print(" ");
// 	}
// 	Serial1.println("\n----\n");
// 	mmc::writeSector(buffer, WATER_STORAGE_PAGE);
// 	mmc::writeSector(buffer, SECOND_COPY_START + WATER_STORAGE_PAGE);

	return true;
}

bool Configuration::readGlobalConfig()
{
// 	File cfg = SD.open("config.cfg");
// 	cfg.seek(0);
	uint8_t buffer[512] = {0};
// 	mmc::readSector(buffer, 0);
//  	Serial1.print("magic:");
//  	Serial1.println(buffer[0], DEC);
	if (buffer[0] != CONFIG_MAGIC) {
// 		EXTRACT_STRING(buffer, wrong_magic);
 		Serial1.println("wrong magic num");
		return false;
	}
// 	magic = mem->read(1);
	memcpy(&this->gcfg, buffer + 2, sizeof(globalConfig));
	if (this->gcfg.pots_count >= MAX_POTS) {
		this->gcfg.pots_count = 0;
	}
	return true;
}

bool Configuration::writeGlobalConfig()
{
	uint8_t buffer[512] = {0};
	buffer[ 0 ] = CONFIG_MAGIC;
	memcpy(buffer + 2, &this->gcfg, sizeof(globalConfig));
// 	mmc::writeSector(buffer, 0);
// 	mmc::writeSector(buffer, SECOND_COPY_START);
	return true;
}

potConfig Configuration::readPot(uint8_t index)
{
	potConfig pc;
	memset(pc.name, 0, POT_NAME_LENGTH);
	if (index >= MAX_POTS || index < 0) {
		Serial1.print("readPot:index ");
		Serial1.print(index, DEC);
		Serial1.println(" out of bounds");
		return pc;
	}
// 	Serial1.println("prepare sector read");Serial1.flush();
//  	uint8_t buffer[512]={0};
//  	mmc::readSector(buffer, POTS_START_ADDRESS + index);
// 	mmc::readBufferEx((uint8_t*)&pc, sizeof(potConfig), POTS_START_ADDRESS + index, 0);
// Serial1.println("sector read ok");Serial1.flush();
//  	memcpy(&pc, buffer, sizeof(potConfig));
// 	Serial1.println("data copied ok");Serial1.flush();
	pc.name[POT_NAME_LENGTH - 1] = 0;
	return pc;
}

bool Configuration::savePot(uint8_t index, potConfig& pc)
{
	if (index >= MAX_POTS || index < 0) {
		Serial1.print("savePot:index ");
		Serial1.print(index, DEC);
		Serial1.println(" out of bounds");
		return false;
	}
	uint8_t buffer[512]={0};
	memcpy(buffer, &pc, sizeof(potConfig));
// 	mmc::writeSector(buffer, POTS_START_ADDRESS + index);
// 	mmc::writeSector(buffer, SECOND_COPY_START + POTS_START_ADDRESS + index);
// 	uint16_t addr = POTS_START_ADDRESS + index * sizeof(potConfig);
// 	File cfg = SD.open("config.cfg", O_WRITE);
// 	cfg.seek(addr);
// 	cfg.write((const uint8_t*)&pc,  sizeof(potConfig));
// 	cfg.close();

#if 0
	mem->writeBuffer(addr, (char*)&pc, sizeof(potConfig));
#endif
	return true;
}

/*
expanderConfig Configuration::readExpander(uint8_t index)
{
	expanderConfig ec;
	mem->readBuffer((EXPANDERS_START_PAGE + index) * MEM_PAGE_SIZE, (char*)&ec, sizeof(ec));
	return ec;
}

bool Configuration::writeExpander(uint8_t index, expanderConfig& ec)
{
	mem->writeBuffer((EXPANDERS_START_PAGE + index) * MEM_PAGE_SIZE, (char*)&ec, sizeof(ec));
	return true;
}*/

void Configuration::read_watering_program(uint8_t pot_index, wateringProgram& wpgm)
{
// 	uint8_t buffer[512]={0};
// 	mmc::readSector(buffer, WATER_PROGRAMS_START_ADDRESS + pot_index);
// 	mmc::readBufferEx((uint8_t*)&wpgm, sizeof(wateringProgram), WATER_PROGRAMS_START_ADDRESS + pot_index, 0);
// 	memcpy(&wpgm, buffer, sizeof(wateringProgram));
}

void Configuration::write_watering_program(uint8_t pot_index, wateringProgram& wpgm)
{
	uint8_t buffer[512]={0};
	memcpy(buffer, &wpgm,  sizeof(wateringProgram));
// 	mmc::writeSector(buffer, WATER_PROGRAMS_START_ADDRESS + pot_index);
// 	mmc::writeSector(buffer, SECOND_COPY_START + WATER_PROGRAMS_START_ADDRESS + pot_index);
}

/*
int16_t Configuration::__get_stat_offset(uint8_t pot_index, uint16_t date)
{
	uint16_t offset;
	mmc::readBuffer((byte*)&offset, sizeof(uint16_t), POTS_STAT_START_BLOCK + pot_index, 0);
	Serial1.print("offset:");
	Serial1.println(offset, DEC);
	return 2 + (date - offset) * 2;
}

bool Configuration::save_stat(uint8_t pot_index, uint16_t value, bool overwrite)
{
	DateTime now = clock.now();
	uint16_t offset = __get_stat_offset(pot_index, __datetime2uint16(now));
	if (!overwrite) {
		uint16_t tmp = 0;
		mmc::readBuffer((byte*)&tmp, sizeof(uint16_t), POTS_STAT_START_BLOCK + pot_index, offset);
		value += tmp;
	}
	mmc::writeBuffer((byte*)&value, sizeof(uint16_t), POTS_STAT_START_BLOCK + pot_index, offset);
	return true;
}

void Configuration::restart_stat(uint8_t pot_index)
{
	DateTime now = clock.now();
	uint16_t offset = __get_stat_offset(pot_index, __datetime2uint16(now));
	mmc::writeBuffer((byte*)&offset, sizeof(uint16_t), POTS_STAT_START_BLOCK + pot_index, 0);
	DayWaterSector stat;
	stat.date = 0;
	stat.last_record = -1;
	memset(stat.entries, 0, sizeof(stat.entries));
	mmc::writeSector((uint8_t*)&stat, POTS_DETAIL_STAT_START_BLOCK + pot_index * 2);
	mmc::writeSector((uint8_t*)&stat, POTS_DETAIL_STAT_START_BLOCK + pot_index * 2 + 1);

// 	mmc::writeSector((uint8_t*)&stat, POTS_DETAIL_STAT_START_BLOCK + pot_index * 2);
}
*/
/**
	day detail watering stat blocks:
	offset		name				data
	0..1		date_of_block		date into uint16_t format
	2			last_rec_offset		offset of last record(converted into byte offset like bytes_offset = (last_rec_offset*8 + 3) )
	3..509		stat record			format:
										uint16_t 	time(hours in hi-byte, minutes in low byte),
										uint32_t	sensor_value
										uint16_t	ml
 */

// void Configuration::day_stat_move2next_record(uint8_t pot_index)
// {
// 	uint32_t sector = POTS_DETAIL_STAT_START_BLOCK + pot_index * 2;
// 	DayWaterSector stat;
// 	mmc::readSector((uint8_t*) &stat, sector);
// 	DateTime now = clock.now();
// 	if (stat.date == __datetime2uint16(now) || stat.date == 0) {
// 		stat.date == __datetime2uint16(now);
// 	} else {
// 		++sector;
// 		mmc::readSector((uint8_t*) &stat, sector);
// 	}
// 	++stat.last_record;
// 	stat.entries[ stat.last_record ].time = now.hours() << 8 | now.minutes();
// 	stat.entries[ stat.last_record ].sensor_value = 0;
// 	stat.entries[ stat.last_record ].ml = 0;
// 	mmc::writeSector((byte*)&stat, sector);
// }
/*
void Configuration::day_stat_save_sensor_value(uint8_t pot_index, uint32_t sensor_value)
{
	uint32_t sector = POTS_DETAIL_STAT_START_BLOCK + pot_index * 2;
	DayWaterSector stat;
	mmc::readSector((uint8_t*) &stat, sector);
	DateTime now = clock.now();
	if (stat.date == __datetime2uint16(now) || stat.date == 0) {
		stat.date == __datetime2uint16(now);
	} else {
		++sector;
		mmc::readSector((uint8_t*) &stat, sector);
	}
	++stat.last_record;
	stat.entries[ stat.last_record ].time = now.hour() << 8 | now.minute();
	stat.entries[ stat.last_record ].sensor_value = sensor_value;
	mmc::writeSector((byte*)&stat, sector);
}

void Configuration::day_stat_save_ml(uint8_t pot_index, uint32_t ml)
{
	uint32_t sector = POTS_DETAIL_STAT_START_BLOCK + pot_index * 2;
	DayWaterSector stat;
	mmc::readSector((uint8_t*) &stat, sector);
	DateTime now = clock.now();
	if (stat.date == __datetime2uint16(now) || stat.date == 0) {
		stat.date == __datetime2uint16(now);
	} else {
		++sector;
		mmc::readSector((uint8_t*) &stat, sector);
	}
	stat.entries[ stat.last_record ].ml = ml;
	mmc::writeSector((byte*)&stat, sector);
	this->save_stat(pot_index, ml, false);
}
*/

/*
bool Configuration::save_day_entry(uint8_t pot_index, uint32_t sensor_value, uint16_t ml)
{
// 	uint16_t day_of_block = 0;
	uint32_t sector = POTS_DETAIL_STAT_START_BLOCK + pot_index * 2;
	DayWaterSector stat;
	mmc::readSector((uint8_t*) &stat, sector);
	DateTime now = clock.now();
	if (stat.date == __datetime2uint16(now) || stat.date == 0) {
		stat.date == __datetime2uint16(now);
	} else {
		++sector;
		mmc::readSector((uint8_t*) &stat, sector);
	}
	++stat.last_record;
	stat.entries[ stat.last_record ].ml = ml;
	stat.entries[ stat.last_record ].sensor_value = sensor_value;
	stat.entries[ stat.last_record ].time = now.hour() << 8 | now.minute();
	mmc::writeSector((uint8_t*) &stat, sector);
	mmc::writeSector((uint8_t*) &stat, SECOND_COPY_START + sector);
}
*/
/*
uint16_t Configuration::__datetime2uint16(DateTime&dt)
{
	return dt.dayIndex();
}

uint16_t Configuration::read_stat_entry(uint8_t pot_index, DateTime& date)
{
	uint16_t offset = __get_stat_offset(pot_index, __datetime2uint16(date)), value = 0;
	mmc::readBuffer((byte*)&value, sizeof(uint16_t), POTS_STAT_START_BLOCK + pot_index, offset);
	return value;
}

*/

void Configuration::midnight_tasks()
{
/*
*/
}

Configuration g_cfg;

