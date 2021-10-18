#ifndef Note_h
#define Note_h

#include <Adafruit_NeoTrellisM4.h>
#include "Cell.h"

class Note: public Cell
{
  public:
    Note();
    int midi;
    bool is_accented;
    void set_note(int midi_note);
    void play();
    void stop();
    void toggle_accent();
};

#endif