//#define DEBUG

#include <stdlib.h>
#include <avr/interrupt.h>
#include <Arduino.h>
#include <ShiftOut.h>
// #define DEBUG 1
#ifdef DEBUG
#include <HardwareSerial.h>
#endif

/*
  ShiftOut.cpp - 74HC959 Control Library
  Author: GusPS, http://gusps.blogspot.com/
  Copyright (c) 2009.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

ShiftOut::ShiftOut(uint8_t latchPin, uint8_t clockPin, uint8_t dataPin, uint8_t registersCount)
	: _latchPin(latchPin), _clockPin(clockPin), _dataPin(dataPin), _registersCount(registersCount)
{
//i have some bugs with my code and moved initialization into init() method. Sorry.
}

void ShiftOut::init()
{
	pinMode(_latchPin, OUTPUT);
	pinMode(_clockPin, OUTPUT);
	pinMode(_dataPin, OUTPUT);

	_values = (uint8_t*) malloc(_registersCount * sizeof(uint8_t));

	_createdOk = (_values != 0);

	for (int i = 0; i < _registersCount; i++) {
		_values[i] = 0;
	}
	sendData();
}

bool ShiftOut::isOk()
{
	return _createdOk;
}

void ShiftOut::digitalWrt(uint8_t pin, int value)
{
	setData(pin, value);
	sendData();
}


void ShiftOut::digitalWrtFull(uint8_t regis, int fullValue)
{
	_values[regis] = fullValue;
	sendData();
}

void ShiftOut::setData(uint8_t pin, int value, bool flush)
{
	uint8_t regis = pin / 8;
	uint8_t bit = pin % 8;
	if (value == HIGH)
		_values[regis] |= 1<<bit;
	else
		_values[regis] &= ~(1<<bit);
	if(flush) {
		sendData();
	}
}

void ShiftOut::sendData()
{
	if (_createdOk)
	{
#ifndef USE_USER_FUNCTION_OVERRIDES
		digitalWrite(_latchPin, 0);
		digitalWrite(_dataPin, 0);
#else
		_digitalWrite(_latchPin, 0);
		_digitalWrite(_dataPin, 0);
#endif
		for (int i = _registersCount - 1; i >= 0; i--)
		{
#ifdef DEBUG
				Serial.print("R");
				Serial.print(i + 1);
				Serial.print(" || ");
#endif
			uint8_t value = _values[i];
			for (int j = 7; j >= 0; j--)
			{
#ifdef DEBUG
				Serial.print("Q");
				Serial.print(j);
				Serial.print("=");
#endif

#ifndef USE_USER_FUNCTION_OVERRIDES
				digitalWrite(_clockPin, 0);
#else
				_digitalWrite(_clockPin, 0);
#endif
			    if (value & (1<<j))
				{
#ifdef DEBUG
					Serial.print("1 | ");
#endif

#ifndef USE_USER_FUNCTION_OVERRIDES
					digitalWrite(_dataPin, 1);
#else
					_digitalWrite(_dataPin, 1);
#endif
				} else {
#ifdef DEBUG
					Serial.print("0 | ");
#endif

#ifndef USE_USER_FUNCTION_OVERRIDES
					digitalWrite(_dataPin, 0);
#else
					_digitalWrite(_dataPin, 0);
#endif
				}
#ifndef USE_USER_FUNCTION_OVERRIDES
			    digitalWrite(_clockPin, 1);
				digitalWrite(_dataPin, 0);
#else
			    _digitalWrite(_clockPin, 1);
				_digitalWrite(_dataPin, 0);
#endif
			}
#ifdef DEBUG
			Serial.println("");
#endif
		}
#ifndef USE_USER_FUNCTION_OVERRIDES
		digitalWrite(_clockPin, 0);
		digitalWrite(_latchPin, 1);
#else
		_digitalWrite(_clockPin, 0);
		_digitalWrite(_latchPin, 1);
#endif
	}
}
