#ifndef Cell_h
#define Cell_h

#include "Arduino.h"

class Cell
{
  public: 
    Cell();
    void toggle();
    void on();
    void off();

  protected:
    bool _is_on;
};

#endif