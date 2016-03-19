#ifndef _ESP8266_INCLUDED_
#define _ESP8266_INCLUDED_

#include <HardwareSerial.h>
#include "config_defines.h"

class ESP8266{
protected:
	HardwareSerial*com;
	uint32_t baud;
	uint8_t rst_pin;
	bool sendCmd(char*cmd, bool wait_reply=false, char*reply_ok=NULL, uint32_t timeout=4000);
	char esp_buf[ESP_BUF_SIZE];
	void (*processPacket)(char*);
	int8_t reset();
public:
	ESP8266(HardwareSerial*_com);
	~ESP8266();
	void begin(uint32_t baud,uint8_t _rst);
	bool connect(uint32_t timeout=60000UL);
	bool sendStr(const char*str);
	void setPacketParser(void (*func)(char*));
};

#endif