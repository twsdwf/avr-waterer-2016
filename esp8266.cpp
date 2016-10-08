#include<Arduino.h>
#include "esp8266.h"

// static PGM_P s_OK = s_OK;

ESP8266::ESP8266(HardwareSerial*_com)
{
	com = _com;
	baud = 0;
	state = enWaitIPD;
}

ESP8266::~ESP8266()
{
	
}

void ESP8266::begin(uint32_t baud,uint8_t _rst)
{
	com->begin(baud);
	this->baud = baud;
	this->rst_pin = _rst;
	pinMode(this->rst_pin, OUTPUT);
	digitalWrite(this->rst_pin, LOW);
	bi = 0;
}

bool ESP8266::sendCmd_P(PGM_P cmd, bool wait_reply, PGM_P reply_ok, uint32_t timeout)
{
// 	int16_t bi = 0;
	while (com->available() > 0 ){
		esp_buf[ bi ] = com->read();
		if (esp_buf[bi] >=' ' || esp_buf[bi] == 10) {
			bi = (bi + 1) % ESP_BUF_SIZE;
		}
	}
	esp_buf[ bi ] = 0;
 	Serial1.print(F("sendCmd_P:"));
	Serial1.println((__FlashStringHelper*)cmd);
	com->println((__FlashStringHelper*)cmd);
	if (!wait_reply) {
		return true;
	}
	bi = 0;
	uint32_t start = millis();
	int old_bi=-1;
	char *ipd = NULL;
	while (millis() - start < timeout) {
		while (com->available() > 0 ) {
			esp_buf[ bi ] = com->read();
			if (esp_buf[bi] >= ' ' || esp_buf[bi] == 10) {
				bi = (bi + 1) % ESP_BUF_SIZE;
			}
		}
		esp_buf[bi] = 0;
		Serial1.print("read:");
		Serial1.println(esp_buf);
		if (strstr_P(esp_buf, PSTR("ready")) /*&& !strstr_P("RST", cmd)*/) {
			start = millis();
			bi = 0;
			esp_buf[0] = 0;
			esp_buf[1] = 0;
			com->println((__FlashStringHelper*)cmd);
			continue;
		}
		if (strstr_P(esp_buf, PSTR("ERROR")) || strstr_P(esp_buf, PSTR("FAIL"))) {
 			if (old_bi != bi) Serial1.println(esp_buf);
			return false;
		}
		/*shit, but often happens*/
		esp_buf[ bi ] = 0;
		ipd = strstr_P(esp_buf, PSTR("+IPD"));

		if (ipd != NULL && strstr_P(esp_buf, PSTR(";")) > ipd) {
//  			memmove(esp_buf, ipd + 4, strlen(ipd) + 1 - 4);
//  			Serial.print("got cnx over cmd [");
//  			Serial.print(ipd + 4);
//  			Serial.println("]");
			processPacket(ipd + 4);
			return false;
		}

		if (reply_ok != NULL) {
			esp_buf[bi] = 0;
			if (old_bi != bi) {
  				Serial1.print(F("got reply:["));
  				Serial1.print(esp_buf);
  				Serial1.println(F("]"));
			}
			char *ptr = strstr_P(esp_buf, reply_ok);
			if (ptr != NULL) {
// 				last_ping = millis();
				return true;
			}
		} else {
// 			last_ping = millis();
			return true;
		}
		old_bi = bi;
	}
//  	Serial1.print((__FlashStringHelper*)cmd);
//  	Serial1.println(F(" command timeout"));
	return false;
}

void ESP8266::process()
{
	char ch;
    while (com->available() > 0) {
        ch = com->read();
        Serial1.print(ch);
        if (state == enWaitIPD && ch == '+') {
// 			Serial.println("got +");
          state = enWaitIPD_char1;
          continue;
        } else if (state == enWaitIPD_char1 && ch == 'I') {
// 			Serial.println("got I");
          state = enWaitIPD_char2;
          continue;
        } else if (state == enWaitIPD_char2 && ch == 'P') {
// 			Serial.println("got P");
          state = enWaitIPD_char3;
          continue;
        } else if (state == enWaitIPD_char3 && ch == 'D') {
// 			Serial.println("Got D. packet starts...");
          state = enReadPacket;
          bi = 0;
          continue;
        } else if (state == enReadPacket) {
          esp_buf[bi++] = ch;
		  if (strstr_P(esp_buf, PSTR("CONNECT")) || strstr_P(esp_buf, PSTR("CLOSED"))||strstr_P(esp_buf,PSTR("+IPD"))) {
			bi = 0;  
		  }
		  if (bi >= sizeof(esp_buf)) {
			Serial1.println(F("esp buf overflow"));
			bi = 0;
		  }
  		  Serial.print("esp_buf [");Serial.print(esp_buf);Serial.println("]");
          if (ch == ';') {
            esp_buf[bi] = 0;
// 			Serial.println("packet read ok");
            processPacket(esp_buf);
            state = enWaitIPD;
			bi=0;
          }
        } else {//if states
			state = enWaitIPD;
		}
    }//while avail

}

bool ESP8266::sendCmd(char*cmd, bool wait_reply, char*reply_ok, uint32_t timeout, bool dbg)
{
	int16_t bi = 0;
	while (com->available() > 0 ){
		esp_buf[ bi ] = com->read();
		if (esp_buf[bi] >=' ' || esp_buf[bi] == 10) {
			bi = (bi + 1) % ESP_BUF_SIZE;
		}
	}
	esp_buf[ bi ] = 0;
	if (1||dbg) {
		Serial1.print(F("send cmd:"));
		Serial1.println(cmd);
	}
	com->println(cmd);
	
	if (!wait_reply) {
		return true;
	}
	bi = 0;
	uint32_t start = millis();
	int old_bi=-1;
	char *ipd = NULL;
	while (millis() - start < timeout) {
		while (com->available() > 0 ) {
			esp_buf[ bi ] = com->read();
			if (esp_buf[bi] >= ' ' || esp_buf[bi] == 10) {
				bi = (bi + 1) % ESP_BUF_SIZE;
			}
		}
		esp_buf[bi] = 0;
		if (strstr_P(esp_buf, PSTR("ready")) /*&& !strstr_P(cmd, PSTR("RST"))*/) {
			start = millis();
			bi = 0;
			esp_buf[0] = 0;
			esp_buf[1] = 0;
			com->println(cmd);
			continue;
		}
		if (strstr_P(esp_buf, PSTR("ERROR")) || strstr_P(esp_buf, PSTR("FAIL"))) {
 			if (dbg && old_bi != bi) Serial1.println(esp_buf);
			return false;
		}
		/*shit, but often happens*/
		esp_buf[ bi ] = 0;
		ipd = strstr_P(esp_buf, PSTR("+IPD"));

		if (ipd != NULL && strstr_P(esp_buf, PSTR(";")) > ipd) {
//  			memmove(esp_buf, ipd + 4, strlen(ipd) + 1 - 4);
			if (dbg) {
				Serial1.print(F("got cnx over cmd ["));
				Serial1.print(ipd + 4);
				Serial1.println(F("]"));
			}
			processPacket(ipd + 4);
			return false;
		}

		if (reply_ok != NULL) {
			esp_buf[bi] = 0;
			if (old_bi != bi && dbg) {
  				Serial1.print(F("got reply:["));
  				Serial1.print(esp_buf);
  				Serial1.println(F("]"));
			}
			char *ptr = strstr(esp_buf, reply_ok);
			if (ptr != NULL) {
// 				last_ping = millis();
				return true;
			}
		} else {
// 			last_ping = millis();
			return true;
		}
		old_bi = bi;
	}
	if (dbg) {
		Serial1.print(cmd);
		Serial1.println(F(" command timeout"));
	}
	return false;
}

int8_t ESP8266::reset()
{

	digitalWrite(this->rst_pin, HIGH);
	delay(200);
	digitalWrite(this->rst_pin, LOW);
	uint8_t bi = 0;
	uint32_t start = millis();
	char ch;
	while(millis() - start <= 10000) {
		while(com->available()) {
			ch = com->read();
			if (ch >=' ' || ch == 10 || ch == 13) {
				esp_buf[ bi++ ] = ch;
			}
		}
		esp_buf[ bi ] = 0;
	}
	int ret = 0;
	if (strstr_P(esp_buf, PSTR("ready"))) {
		ret = 1;
	}
	if (strstr_P(esp_buf, PSTR("WIFI GOT IP"))) {
		ret = 2;
	}
	return ret;	
}


bool ESP8266::connect(uint32_t timeout)
{
	do {
		bool ok = sendCmd_P(PSTR("AT"), true, s_OK, 4000);
		int ret = 1;
		if (!ok) {
			do {
				ret = this->reset();
			} while(ret == 0);
		}
		ok = sendCmd_P(PSTR("AT"), true, s_OK, 4000);
		if (ok) {
 			Serial1.println(F("AT is OK"));
		}
	// 	sendCmd("AT+RST", false, 0, 5000);
	// 	delay(1000);
		sendCmd_P(PSTR("ATE0"), false, 0, 5000);
		sendCmd_P(PSTR("AT+CIFSR"), true, s_OK, 5000);
	 	Serial1.print(F("ret="));Serial1.println(ret, DEC);
// 		delay(5000);
		if (ret < 2) {
			if (sendCmd_P(PSTR("AT+CWQAP"), true, s_OK, 5000) &&
			sendCmd_P(PSTR("AT+CWMODE=1"), true, s_OK, 5000) &&
			sendCmd_P(PSTR("AT+CWJAP=\"Pi\",\"kopernik1533\""), true, PSTR("WIFI GOT IP"), 20000)) {
				sendCmd_P(PSTR("AT+CWSAP=\"AI-water\",\"BvftghjKIUYGc56(&^Tfc\",9,3"), true, s_OK, 5000);
			} else {
				continue;
			}
		}

		sendCmd_P(PSTR("AT+CIPSERVER=0"), true, s_OK, 5000);
		sendCmd_P(PSTR("AT+CIPMODE=0"), true, s_OK, 5000);
		sendCmd_P(PSTR("AT+CIPMUX=1"), true, s_OK, 5000);

		if (sendCmd_P(PSTR("AT+CIPSERVER=1,80"), true, s_OK, 5000)) {
	 		Serial1.println(F("TCP SRV OK"));
			break;
		} else {
			continue;
		}
	} while (true);
	return false;
}

bool ESP8266::sendStr(const char*str)
{
	Serial1.print(F("send str:"));
	Serial1.print(str);
	com->print(str);
	com->write(0);
	return false;
}

void ESP8266::setPacketParser(void (*func)(char*))
{
	this->processPacket = func;
}

DateTime ESP8266::getTimeFromNTPServer()
{
	const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
	uint8_t packetBuffer[ NTP_PACKET_SIZE];
// 	Serial.println("sending NTP packet...");
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011; // LI, Version, Mode
	packetBuffer[1] = 0; // Stratum, or type of clock
	packetBuffer[2] = 6; // Polling Interval
	packetBuffer[3] = 0xEC; // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	char buff[20];

	sendCmd_P(PSTR("AT+CIPCLOSE=3"), true, s_OK, 1000);
	sendCmd_P(PSTR("AT+CIPSTART=3,\"UDP\",\"129.6.15.28\",123"), true, s_OK, 3000) &&
	sprintf(buff, "AT+CIPSEND=3,%d", NTP_PACKET_SIZE);
	if (sendCmd(buff, true, ">", 1000)) {
		for (uint8_t i=0; i< NTP_PACKET_SIZE; ++i) {
			com->write(packetBuffer[i]);
		}
		Serial1.println(F("NTP query sent"));
	} else {
		Serial1.println(F("NTP query fault"));
		return DateTime(0);
	}
	uint8_t i = 0,bWr = 0;
	uint32_t start = millis();

	do {
		while (com->available()) {
			char ch = com->read();
			if (bWr == 0 && ch == '+') ++bWr;
			else if (bWr == 1 && ch == 'I') ++bWr;
			else if (bWr == 2 && ch == 'P') ++bWr;
			else if (bWr == 3 && ch == 'D') ++bWr;
			else if (bWr == 4 && ch == ',') ++bWr;
			else if (bWr == 5 && ch == '3') ++bWr;
			else if (bWr == 6 && ch == ',') ++bWr;
			else if (bWr == 7 && ch == '4') ++bWr;
			else if (bWr == 8 && ch == '8') ++bWr;
			else if (bWr == 9 && ch == ':') ++bWr;
			else if (bWr == 10){
				packetBuffer[i++] = ch;
				if (i >= NTP_PACKET_SIZE) {
					bWr = 42;
					break;
				}
			}
		}
	} while (millis() - start < 4000 && bWr!=42);
	if (bWr == 42) {
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
//#define SECONDS_FROM_1970_TO_2000 946684800

		unsigned long epoch = secsSince1900 - seventyYears - 946684800 + 10800;//UTC+3;
		
		return DateTime(epoch);
	}//while
	Serial1.println("error while time get");
	return DateTime(0);
}