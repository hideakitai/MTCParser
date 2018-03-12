#include "MTCParser.h"

MTCParser mtc;

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    while (const size_t size = Serial.available())
    {
        uint8_t data[size];
        Serial.readBytes((char*)data, size);

        mtc.feed(data, size);

        if (mtc.available())
        {
            Serial.print("MTC    : ");
            Serial.println(mtc.asString());
            Serial.print("Type   : ");
            Serial.println(mtc.type());
            Serial.print("Frame  : ");
            Serial.println(mtc.asFrameCount());
            Serial.print("Second : ");
            Serial.println(mtc.asSeconds());

            mtc.pop();
        }
    }
}
