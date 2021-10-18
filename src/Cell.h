#ifndef Cell_h
#define Cell_h

#include "Arduino.h"

class Cell
{
  public: 
    Cell();
    bool is_on;
    void toggle();
    void on();
    void off();
};

#endif