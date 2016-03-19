#include<Arduino.h>
#include "esp8266.h"

// static PGM_P s_OK = "OK";

ESP8266::ESP8266(HardwareSerial*_com)
{
	com = _com;
	baud = 0;
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
}

bool ESP8266::sendCmd_P(PGM_P cmd, bool wait_reply, PGM_P reply_ok, uint32_t timeout)
{
	int16_t bi = 0;
	while (com->available() > 0 ){
		esp_buf[ bi ] = com->read();
		if (esp_buf[bi] >=' ' || esp_buf[bi] == 10) {
			bi = (bi + 1) % ESP_BUF_SIZE;
		}
	}
	esp_buf[ bi ] = 0;
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
		if (strstr_P(esp_buf, PSTR("ready")) && !strstr_P(cmd, PSTR("RST"))) {
			start = millis();
			bi = 0;
			esp_buf[0] = 0;
			esp_buf[1] = 0;
			com->println(cmd);
			continue;
		}
		if (strstr_P(esp_buf, PSTR("ERROR")) || strstr_P(esp_buf, PSTR("FAIL"))) {
// 			if (old_bi != bi) Serial.println(esp_buf);
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
//  				Serial.print("got reply:[");
//  				Serial.print(esp_buf);
//  				Serial.println("]");
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
// 	Serial.print(cmd);
// 	Serial.println(" command timeout");
	return false;
}


bool ESP8266::sendCmd(char*cmd, bool wait_reply, char*reply_ok, uint32_t timeout)
{
	int16_t bi = 0;
	while (com->available() > 0 ){
		esp_buf[ bi ] = com->read();
		if (esp_buf[bi] >=' ' || esp_buf[bi] == 10) {
			bi = (bi + 1) % ESP_BUF_SIZE;
		}
	}
	esp_buf[ bi ] = 0;
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
		if (strstr_P(esp_buf, PSTR("ready")) && !strstr_P(cmd, PSTR("RST"))) {
			start = millis();
			bi = 0;
			esp_buf[0] = 0;
			esp_buf[1] = 0;
			com->println(cmd);
			continue;
		}
		if (strstr_P(esp_buf, PSTR("ERROR")) || strstr_P(esp_buf, PSTR("FAIL"))) {
// 			if (old_bi != bi) Serial.println(esp_buf);
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
//  				Serial.print("got reply:[");
//  				Serial.print(esp_buf);
//  				Serial.println("]");
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
// 	Serial.print(cmd);
// 	Serial.println(" command timeout");
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
		bool ok = sendCmd("AT", true, "OK", 4000);
		int ret = 1;
		if (!ok) {
			do {
				ret = this->reset();
			} while(ret == 0);
		}
		ok = sendCmd("AT", true, "OK", 4000);
		if (ok) {
// 			Serial.println("AT is OK");
		}
	// 	sendCmd("AT+RST", false, 0, 5000);
	// 	delay(1000);
		sendCmd("ATE0", false, 0, 5000);
		sendCmd("AT+CIFSR", true, "OK", 5000);
	// 	Serial.print("ret=");Serial.println(ret, DEC);
		delay(5000);
		if (ret < 2) {
			if (sendCmd("AT+CWQAP", true, "OK", 5000) &&
			sendCmd("AT+CWMODE=1", true, "OK", 5000) &&
			sendCmd("AT+CWJAP=\"Pi\",\"kopernik1533\"", true, "WIFI GOT IP", 20000)) {
				sendCmd("AT+CWSAP=\"AI-water\",\"BvftghjKIUYGc56(&^Tfc\",9,3", true, "OK", 5000);
			} else {
				continue;
			}
		}

		sendCmd("AT+CIPSERVER=0", true, "OK", 5000);
		sendCmd("AT+CIPMODE=0", true, "OK", 5000);
		sendCmd("AT+CIPMUX=1", true, "OK", 5000);

		if (sendCmd("AT+CIPSERVER=1,80", true, "OK", 5000)) {
	// 		Serial.println("TCP SRV OK");
			break;
		} else {
			continue;
		}
	} while (true);
	return false;
}

bool ESP8266::sendStr(const char*str)
{
	return false;
}

void ESP8266::setPacketParser(void (*func)(char*))
{
	this->processPacket = func;
}
