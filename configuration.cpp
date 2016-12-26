
#include "Arduino.h"

#include "awtypes.h"
#include "config_defines.h"
// #include "mmc.h"
#include "configuration.h"

#include "RTClib.h"

extern RTC_DS1307 clock;
// static char wrong_magic[] PROGMEM = "bad magic!";
extern AT24Cxxx mem;

Configuration::Configuration()
{
	memset(&this->config, 0, sizeof(config));
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
	memset(wsd, 0, sizeof(WaterStorageData));
	mem.readBuffer((WATERSTORAGE_PAGE0 + index) * MEM_PAGE_SIZE, (char*)wsd, sizeof(WaterStorageData));
	return true;
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
	return buffer[0];
}

void Configuration::setWScount(uint8_t count)
{

}

bool Configuration::writeWaterStorageData(WaterStorageData*wsd, uint8_t index)
{
	mem.writeBuffer((WATERSTORAGE_PAGE0 + index) * MEM_PAGE_SIZE, (char*)wsd, sizeof(WaterStorageData));
	return true;
}

bool Configuration::readGlobalConfig()
{
	uint8_t buffer[32] = {0};
	mem.readBuffer(GCFG_ADDRESS, (char*)buffer, 32);
	if (buffer[0] != CONFIG_MAGIC) {
 		Serial1.println(F("wrong magic num"));
		return false;
	}
	memcpy(&this->config, buffer + 2, sizeof(globalConfig));
	if (this->config.pots_count >= MAX_POTS) {
		this->config.pots_count = 0;
	}
	return true;
}

bool Configuration::writeGlobalConfig()
{
	uint8_t buffer[32] = {0};
	buffer[ 0 ] = CONFIG_MAGIC;
	buffer[ 1 ] = CONFIG_VERSION;
	memcpy(buffer + 2, &this->config, sizeof(globalConfig));
	mem.writeBuffer(GCFG_ADDRESS, (char*)buffer, 32);
	return true;
}

potConfig Configuration::readPot(uint8_t index)
{
	potConfig pc;
	memset(pc.name, 0, POT_NAME_LENGTH);
	if (index >= MAX_POTS || index < 0) {
		Serial1.print(F("readPot:index "));
		Serial1.print(index, DEC);
		Serial1.println(F(" out of bounds"));
		return pc;
	}
	mem.readBuffer(MEM_PAGE_SIZE *(POTS_DATA_PAGE0 + index), (char*)&pc, sizeof(pc));
	pc.name[POT_NAME_LENGTH - 1] = 0;
	return pc;
}

bool Configuration::savePot(uint8_t index, potConfig& pc)
{
	if (index >= MAX_POTS || index < 0) {
		Serial1.print(F("savePot:index "));
		Serial1.print(index, DEC);
		Serial1.println(F(" out of bounds"));
		return false;
	}
	mem.writeBuffer(MEM_PAGE_SIZE *(POTS_DATA_PAGE0 + index), (char*)&pc, sizeof(potConfig));
	return true;
}

void Configuration::read_watering_program(uint8_t pot_index, wateringProgram& wpgm)
{
}

void Configuration::write_watering_program(uint8_t pot_index, wateringProgram& wpgm)
{
	uint8_t buffer[512]={0};
	memcpy(buffer, &wpgm,  sizeof(wateringProgram));
}

void Configuration::midnight_tasks()
{
/*
*/
}

Configuration g_cfg;
char*str = (char*)malloc(100);
