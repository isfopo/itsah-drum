#ifndef Note_h
#define Note_h

#include <Adafruit_NeoTrellisM4.h>
#include "Cell.h"

class Note: public Cell
{
  public:
    Note(int midi_note, Adafruit_NeoTrellisM4 trellis);
    void play();
    void stop();
    void toggle_accent();

  private:
    Adafruit_NeoTrellisM4 _trellis;
    int _midi_note;
    bool _is_accented;
};

#endif