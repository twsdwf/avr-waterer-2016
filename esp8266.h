#ifndef _ESP8266_INCLUDED_
#define _ESP8266_INCLUDED_

#include <HardwareSerial.h>
#include "config_defines.h"
#include "RTClib.h"

typedef enum enESPState{
    enWaitIPD,
    enWaitIPD_char1,
    enWaitIPD_char2,
    enWaitIPD_char3,
    enWaitIPD_char4,
    enReadPacket
}enESPState;

class ESP8266{
protected:
	enESPState state;
	HardwareSerial*com;
	uint32_t baud;
	uint8_t bi;
	uint8_t rst_pin;
	char esp_buf[ESP_BUF_SIZE];
	void (*processPacket)(char*);
	int8_t reset();
public:
	bool sendCmd_P(PGM_P cmd, bool wait_reply=false, PGM_P reply_ok=NULL, uint32_t timeout=4000);
	bool sendCmd(char*cmd, bool wait_reply=false, char*reply_ok=NULL, uint32_t timeout=4000, bool dbg=false);
	bool sendZeroChar(char*reply_ok, uint32_t timeout=4000);
	ESP8266(HardwareSerial*_com);
	~ESP8266();
	void begin(uint32_t baud,uint8_t _rst);
	bool connect(uint32_t timeout=60000UL);
	bool sendStr(const char*str);
	void setPacketParser(void (*func)(char*));
	void process();
	DateTime getTimeFromNTPServer();
};

#endif