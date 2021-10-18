#include "Note.h"

void Note::set_note(int midi_note)
{
  midi = midi_note;
}


void Note::toggle_accent()
{
  is_accented = !is_accented;
};