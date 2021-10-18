#include "Note.h"

Note::Note(int midi_note, Adafruit_NeoTrellisM4 trellis)
{
  _midi_note = midi_note;
  _trellis = trellis;
};

void Note::play()
{
  if (_is_on) {
    if (_is_accented) 
    {
      _trellis.noteOn(_midi_note, 127);
    } else {
      _trellis.noteOn(_midi_note, 96);
    }
  }
}

void Note::stop()
{
  _trellis.noteOff(_midi_note, 0);
}

void Note::toggle_accent()
{
  _is_accented = !_is_accented;
};