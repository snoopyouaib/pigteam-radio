#ifndef FT6336_H
#define FT6336_H

#include <Arduino.h>
#include <Wire.h>

#define FT6336_ADDR 0x38

class FT_Point {
  public:
    FT_Point(void);
    FT_Point(uint8_t id, uint16_t x, uint16_t y, uint8_t size);
    bool operator==(FT_Point);
    bool operator!=(FT_Point);
    uint8_t id;
    uint16_t x;
    uint16_t y;
    uint8_t size;
};

class FT6336 {
  public:
    FT6336(uint8_t _sda, uint8_t _scl, uint8_t _int, uint8_t _rst, uint16_t _width, uint16_t _height);
    void begin();
    void reset();
    void setRotation(uint8_t rot);
    void setResolution(uint16_t _width, uint16_t _height);
    void read(void);

    uint8_t touches = 0;
    bool isTouched = false;
    FT_Point points[5];

  private:
    uint8_t rotation = 0;
    uint8_t addr = FT6336_ADDR;
    uint8_t pinSda;
    uint8_t pinScl;
    uint8_t pinInt;
    uint8_t pinRst;
    uint16_t width;
    uint16_t height;
};

#endif // FT6336_H
