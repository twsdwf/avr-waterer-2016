#include "Arduino.h"
#include "waterstorage.h"
#include "configuration.h"

extern Configuration g_cfg;

void WaterStorages::begin()
{
	return;
// 	storages_count = g_cfg.readWScount();
}

WaterStorageData WaterStorages::readStorageData(uint8_t index)
{
	WaterStorageData result;
	if (index > storages_count) {
		memset(&result, 0, sizeof(result));
		return result;
	}
	g_cfg.readWaterStorageData(&result, index);
	return result;
}

WaterStorageData WaterStorages::readNonemptyStorage(uint8_t& index)
{
	WaterStorageData wsd;
	memset(&wsd, 0, sizeof(wsd));
	for (uint8_t i = 0; i < storages_count; ++i) {
		wsd = this->readStorageData(i);
		if (0==wsd.enabled) continue;
		if (wsd.vol - wsd.spent <= 0) continue;
		index = i;
		return wsd;
	}
	index = 255;
	return wsd;
}

uint8_t WaterStorages::getStoragesCount()
{
	return storages_count;
}

void  WaterStorages::setStorageStateEmpty(uint8_t index)
{
	WaterStorageData result = this->readStorageData(index);
	result.spent = result.vol;
	updateStorageData(index, result);
}

void WaterStorages::setStorageStateFull(uint8_t index)
{
	WaterStorageData result = this->readStorageData(index);
	result.spent = 0;
	updateStorageData(index, result);
}

void WaterStorages::dec(uint8_t index, uint16_t ml)
{
	WaterStorageData result = this->readStorageData(index);
	result.spent += ml;
	updateStorageData(index, result);
}

uint16_t WaterStorages::avail(uint8_t index)
{
	WaterStorageData result = this->readStorageData(index);
	return result.vol - result.spent;
}

void WaterStorages::updateStorageData(uint8_t index, WaterStorageData& data)
{
	g_cfg.writeWaterStorageData(&data, index);
}

uint8_t WaterStorages::addStorage(WaterStorageData& data)
{
	g_cfg.writeWaterStorageData(&data, storages_count);
	++storages_count;
	g_cfg.setWScount(storages_count);
	return storages_count - 1;
}


bool WaterStorages::deleteStorage(uint8_t index)
{
	for (uint8_t i = index;i < storages_count - 2; ++i) {
		WaterStorageData wsd1=readStorageData(i + 1);
		updateStorageData(i, wsd1);
	}
	--storages_count;
	g_cfg.setWScount(storages_count);
	return true;
}
