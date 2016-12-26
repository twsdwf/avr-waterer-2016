#ifndef ShiftOut_h
#define ShiftOut_h

/*
  ShiftOut.h - 74HC595 Control Library
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

  Modified by twsdwf, apr-nov 2013 a.d.

*/

#include <inttypes.h>
/*
	our "fast write" function
*/
#ifdef USE_USER_FUNCTION_OVERRIDES
	extern void _digitalWrite(uint8_t pin, uint8_t value);
#endif
class ShiftOut
{
  public:
	//Creates the object and allocate the necesary memory for data handling. Turn to LOW all the PINs in all IC 595.
	//registersCount param is the amount of IC 595 that will be used is cascade.
	ShiftOut(uint8_t latchPin, uint8_t clockPin, uint8_t dataPin, uint8_t registersCount);
	// added by twsdwf.
	void init();
	//Sets the value of a PIN and send the data to IC 595
    void digitalWrt(uint8_t Pin, int value);
	//Sets the value of a PIN but doesn't send it to IC 595(if flush!true). Useful for batch update and then call the sendData method.
    void setData(uint8_t pin, int value, bool flush = false);
	//Sets all the PINs of a 595 and send the data to it
	void digitalWrtFull(uint8_t regis, int fullValue);
	//Send the data on memory to the 595 ICs
	void sendData();
	//"TRUE" if the memory allocation on constructor was successfully
	bool isOk();

  private:
    uint8_t _latchPin;
	uint8_t _clockPin;
	uint8_t _dataPin;
	uint8_t _registersCount;
	uint8_t *_values;
	bool _createdOk;
};

#endif
