#ifndef Note_h
#define Note_h

#include <Adafruit_NeoTrellisM4.h>
#include "Cell.h"

class Note: public Cell
{
  public:
    Note();
    int midi;
    int key;
    bool is_accented;
    void set_note(int midi_note);
    void set_key(int index);
    void toggle_accent();
};

#endif