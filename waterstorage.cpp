#include "Arduino.h"
#include "waterstorage.h"
#include "configuration.h"

extern Configuration g_cfg;

void WaterStorages::begin()
{
	storages_count = 2;
	return;
}

WaterStorageData WaterStorages::readStorageData(uint8_t index)
{
	WaterStorageData result;
	if (index > getStoragesCount()) {
		memset(&result, 0, sizeof(result));
		return result;
	}
	g_cfg.readWaterStorageData(&result, index);
	return result;
}

WaterStorageData WaterStorages::readNonemptyStorage(uint8_t& index, uint8_t min_ml)
{
	WaterStorageData wsd;
	memset(&wsd, 0, sizeof(wsd));
	for (uint8_t i = 0; i < getStoragesCount(); ++i) {
// 		Serial1.print(F("read storage "));
		Serial1.print(i, DEC);
		 g_cfg.readWaterStorageData(&wsd, i);
		if (0 == wsd.enabled) {
// 			Serial1.println(F("  storage is off"));
			continue;
		} else {
// 			Serial1.print(F(" en:1 "));
		}
// 		Serial1.print(F(" volume:"));
// 		Serial1.print(wsd.vol);
// 		Serial1.print(F(" spent:"));
// 		Serial1.println(wsd.spent);
		if (wsd.vol <= wsd.spent || wsd.vol - wsd.spent < min_ml ) {
// 			Serial1.println(F("  storage is ran out"));
			continue;
		}
		index = i;
		return wsd;
	}
// 	Serial1.println(F("\r\nstorage not found"));
	index = 255;
	return wsd;
}

uint8_t WaterStorages::getStoragesCount()
{
	storages_count = 2;
	return storages_count;
}

void  WaterStorages::setStorageStateEmpty(uint8_t index)
{
	WaterStorageData result = this->readStorageData(index);
	result.spent = result.vol;
	g_cfg.writeWaterStorageData(&result, index);
}

void WaterStorages::setStorageStateFull(uint8_t index)
{
	WaterStorageData result = this->readStorageData(index);
	result.spent = 0;
	g_cfg.writeWaterStorageData(&result, index);
}

void WaterStorages::dec(uint8_t index, uint16_t ml)
{
	if (index < 100) {
		WaterStorageData result;
		g_cfg.readWaterStorageData(&result, index);
		Serial1.print(F("dec storage #"));
		Serial1.print(index, DEC);
		Serial1.print(F(" to "));
		Serial1.print(ml, DEC);
		Serial1.println(F("ml"));
		result.spent += ml;
		g_cfg.writeWaterStorageData(&result, index);
		//updateStorageData(index, result);
	}
}

uint16_t WaterStorages::avail(uint8_t index)
{
	WaterStorageData result = this->readStorageData(index);
	return result.vol - result.spent;
}

uint16_t WaterStorages::availTotal()
{
	uint16_t result = 0;
	for (uint8_t i =0; i < getStoragesCount(); ++i) {
		result += avail(i);
	}
	return result;
}

void WaterStorages::updateStorageData(uint8_t index, WaterStorageData& data)
{
	g_cfg.writeWaterStorageData(&data, index);
}

uint8_t WaterStorages::addStorage(WaterStorageData& data)
{
	return 255;
	g_cfg.writeWaterStorageData(&data, storages_count);
	++storages_count;
	g_cfg.setWScount(storages_count);
	return storages_count - 1;
}


bool WaterStorages::deleteStorage(uint8_t index)
{
	return false;
	for (uint8_t i = index;i < getStoragesCount() - 2; ++i) {
		WaterStorageData wsd1=readStorageData(i + 1);
		updateStorageData(i, wsd1);
	}
	--storages_count;
	g_cfg.setWScount(storages_count);
	return true;
}
