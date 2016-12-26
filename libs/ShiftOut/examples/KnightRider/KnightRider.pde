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
    Reg1.digitalWrt(j, LOW);
  }

  for (int j = 15; j >= 0; j--)
  {
    Reg1.digitalWrt(j, HIGH);
    delay(100);
    Reg1.digitalWrt(j, LOW);
  }
}

