#include <ShiftOut.h>

ShiftOut Reg1(8,12,11,2);

void setup()
{
  delay(1000);
}

void loop()
{
  for (int j = 0; j < 16; j++)
  {
    Reg1.digitalWrt(j, HIGH);
    delay(100);
  }
  Reg1.digitalWrtFull(0,0);
  Reg1.digitalWrtFull(1,0);
  delay(500);
  Reg1.digitalWrtFull(0,B00001111);
  Reg1.digitalWrtFull(1,B11110000);
  delay(500);
  Reg1.digitalWrtFull(0,0);
  Reg1.digitalWrtFull(1,0);
  delay(500);
}
