// Trellis M4 MIDI Keypad CC
// sends 32 notes, pitch bend & a CC from accelerometer tilt over USB MIDI

#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <MIDIUSB.h>
#include "Note.h"

#define MIDI_CHANNEL 0 // default channel # is 0
#define FIRST_MIDI_NOTE 36
#define NUMBER_OF_COLUMNS_ON_TRELLIS 8
#define NUMBER_OF_ROWS_ON_TRELLIS 4

Adafruit_NeoTrellisM4 trellis = Adafruit_NeoTrellisM4();
Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

uint32_t tick = 0;

// colors
uint32_t column_color = 0XEDECEE;
uint32_t shift_column_color = 0XEDECEE;
uint32_t main_color = 0X61FFCA;
uint32_t shift_color = 0xA277FF;
uint32_t ref_color_1 = 0x00F0FF;
uint32_t ref_color_2 = 0xFF00FF;
uint32_t off_color = 0X0;

const int NUMBER_OF_COLUMNS = 8;
const int NUMBER_OF_ROWS = 16;
const int TICKS_IN_MEASURE = 96;

int row_offset = 12;
int last_step = 16;
int swing = 6;

Note main_grid[NUMBER_OF_COLUMNS][NUMBER_OF_ROWS];
Note shift_grid[NUMBER_OF_COLUMNS][NUMBER_OF_ROWS];

boolean pressed_keys[32];
boolean combo_pressed = false;

// modes
boolean main_mode = true;

// combos
int back_combo[] = {7, 28, 31};
int shift_combo[] = {7, 20, 31};
int offset_init_combo[] = {6, 30};
int offset_up_combo[] = {11, 6, 30};
int offset_down_combo[] = {19, 6, 30};
int swing_init_combo[] = {14, 30};
int swing_6_combo[] = {28, 14, 30};
int swing_7_combo[] = {20, 14, 30};
int swing_8_combo[] = {12, 14, 30};
int swing_9_combo[] = {4, 14, 30};

// floating point map
float ofMap(float value, float inputMin, float inputMax, float outputMin, float outputMax, bool clamp)
{
  float outVal = ((value - inputMin) / (inputMax - inputMin) * (outputMax - outputMin) + outputMin);

  if (clamp)
  {
    if (outputMax < outputMin)
    {
      if (outVal < outputMax)
        outVal = outputMax;
      else if (outVal > outputMin)
        outVal = outputMin;
    }
    else
    {
      if (outVal > outputMax)
        outVal = outputMax;
      else if (outVal < outputMin)
        outVal = outputMin;
    }
  }
  return outVal;
}

void play(Note note)
{
  if (note.is_on)
  {
    if (note.is_accented)
    {
      trellis.noteOn(note.midi, 127);
    }
    else
    {
      trellis.noteOn(note.midi, 96);
    }
  }
}

void stop(Note note)
{
  trellis.noteOff(note.midi, 0);
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return trellis.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return trellis.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return trellis.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

uint32_t sixteenthNoteToTicks(uint8_t sixteenthNote)
{
  return sixteenthNote * 6;
}

uint32_t tickToEighthNote(uint32_t tick)
{
  return tick / 12;
}

int coordinatesToKey(int col, int row)
{
  return (row * 8) + col;
}

int getGridNote(int start, int rows, int index)
{
  return (start + rows - 1) - index;
}

boolean isInRangeOfRows(int midi_note) {
  return ((FIRST_MIDI_NOTE + NUMBER_OF_ROWS) - row_offset - 4) <= midi_note && midi_note < ((FIRST_MIDI_NOTE + NUMBER_OF_ROWS) - row_offset);
}

int numberOfButtonPressed(bool pressed_buttons[], int size)
{
  int count = 0;

  for (int i = 0; i < size; ++i)
  {
    if (pressed_buttons[i])
    {
      count++;
    }
  }

  return count;
}

bool checkCombo(int combo[], int size, bool pressed_buttons[])
{
  for (int i = 0; i < size; i++)
  {
    if (!pressed_buttons[combo[i]])
    {
      return false;
    }
  }
  return true;
}

void setup()
{
  Serial.begin(115200);

  trellis.begin();
  trellis.setBrightness(80);

  // USB MIDI messages sent over the micro B USB port
  Serial.println("Enabling MIDI on USB");
  trellis.enableUSBMIDI(true);
  trellis.setUSBMIDIchannel(MIDI_CHANNEL);

  for (int i = 0; i < NUMBER_OF_COLUMNS; i++)
  {
    for (int j = 0; j < NUMBER_OF_ROWS; j++)
    {
      main_grid[i][j] = Note();
      main_grid[i][j].set_note(getGridNote(FIRST_MIDI_NOTE, NUMBER_OF_ROWS, j));
      main_grid[i][j].set_key(coordinatesToKey(i, j) % 32);
    }
  }
  for (int i = 0; i < NUMBER_OF_COLUMNS; i++)
  {
    for (int j = 0; j < NUMBER_OF_ROWS; j++)
    {
      shift_grid[i][j] = Note();
      shift_grid[i][j].set_note(getGridNote(FIRST_MIDI_NOTE, NUMBER_OF_ROWS, j));
      shift_grid[i][j].set_key(coordinatesToKey(i, j) % 32);
    }
  }
}

void loop()
{
  trellis.tick();

  midiEventPacket_t midi_in = MidiUSB.read();

  if (midi_in.header == 3)
  { // transport start

    Serial.println("start");
    tick = sixteenthNoteToTicks(midi_in.byte2); // syncs ticks to transport
  }
  else if (midi_in.header == 15)
  { // tick event - happens 24 times per quarter note

    // play and stop notes
    if (tick % 12 == 0)
    {
      for (Note note : main_grid[tickToEighthNote(tick) % last_step])
      {
        play(note);
      }
      for (Note note : main_grid[(tickToEighthNote(tick) - 1) % last_step])
      {
        stop(note);
      }
    }
    else if ((int)(tick % 12) == swing)
    {
      for (Note note : shift_grid[tickToEighthNote(tick) % last_step])
      {
        play(note);
      }
      for (Note note : shift_grid[(tickToEighthNote(tick) - 1) % last_step])
      {
        stop(note);
      }
    }
    // set lights
    if (tick % 12 == 0)
    {
      for (Note note : main_grid[tickToEighthNote(tick) % 8])
      {
        if (main_mode)
        {
          trellis.setPixelColor(note.key, column_color);
        }
      }
      for (Note note : main_grid[(tickToEighthNote(tick) - 1) % 8])
      {
        if (main_mode)
        {
          if (isInRangeOfRows(note.midi))
          {
            trellis.setPixelColor(note.key, note.is_on ? main_color : off_color);
          }
        }
      }
    }
    else if ((int)(tick % 12) == swing)
    {
      for (Note note : shift_grid[tickToEighthNote(tick) % 8])
      {
        if (!main_mode)
        {
          trellis.setPixelColor(note.key, column_color);
        }
      }
      for (Note note : shift_grid[(tickToEighthNote(tick) - 1) % 8])
      {
        if (!main_mode)
        {
          if (isInRangeOfRows(note.midi))
          {
            trellis.setPixelColor(note.key, note.is_on ? shift_color : off_color);
          }
        }
      }
    }
    tick++;
  }
  else if (midi_in.header == 11)
  { // transport end
    Serial.println("stop");
  }
  else if (midi_in.header != 0)
  {
    Serial.println("message in");
    Serial.println(midi_in.header);
    Serial.println(" ");
    Serial.println(midi_in.byte1);
    Serial.println(" ");
    Serial.println(midi_in.byte2);
    Serial.println(" ");
    Serial.println(midi_in.byte3);
  }

  if (numberOfButtonPressed(pressed_keys, sizeof(pressed_keys)) > 1)
  {
    if (checkCombo(offset_init_combo, sizeof(offset_init_combo) / sizeof(offset_init_combo[0]), pressed_keys))
    {
      // light up offset keys for reference
      trellis.setPixelColor(offset_up_combo[0], ref_color_1);
      trellis.setPixelColor(offset_down_combo[0], ref_color_1);
    }
    else if (checkCombo(swing_init_combo, sizeof(swing_init_combo) / sizeof(swing_init_combo[0]), pressed_keys))
    {
      // light up swing keys for reference
      trellis.setPixelColor(swing_6_combo[0], ref_color_1);
      trellis.setPixelColor(swing_7_combo[0], ref_color_1);
      trellis.setPixelColor(swing_8_combo[0], ref_color_1);
      trellis.setPixelColor(swing_9_combo[0], ref_color_1);
    }
  }

  while (trellis.available())
  {
    keypadEvent e = trellis.read();
    int key = e.bit.KEY;
    int col = e.bit.COL;
    int row = e.bit.ROW;

    if (e.bit.EVENT == KEY_JUST_PRESSED)
    {
      Serial.println(" pressed\n");
      Serial.print("key: ");
      Serial.println(key);
      pressed_keys[key] = true;
      if (numberOfButtonPressed(pressed_keys, sizeof(pressed_keys)) > 1)
      {
        combo_pressed = true;
        if (checkCombo(shift_combo, sizeof(shift_combo) / sizeof(shift_combo[0]), pressed_keys))
        {
          main_mode = false;

          for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
          {
            for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
            {
              trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_color : off_color);
            }
          }
        }
        else if (checkCombo(back_combo, sizeof(back_combo) / sizeof(back_combo[0]), pressed_keys))
        {
          main_mode = true;
          for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
          {
            for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
            {
              trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_color : off_color);
            }
          }
        }
        else if (checkCombo(offset_up_combo, sizeof(offset_up_combo) / sizeof(offset_up_combo[0]), pressed_keys))
        {
          if (row_offset > 0)
          {
            row_offset -= 4;
            if (main_mode)
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_color : off_color);
                }
              }
            }
            else
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_color : off_color);
                }
              }
            }
          }
        }
        else if (checkCombo(offset_down_combo, sizeof(offset_down_combo) / sizeof(offset_down_combo[0]), pressed_keys))
        {
          if (row_offset < 12)
          {
            row_offset += 4;
            if (main_mode)
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_color : off_color);
                }
              }
            }
            else
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_color : off_color);
                }
              }
            }
          }
        }
        else if (checkCombo(swing_6_combo, sizeof(swing_6_combo) / sizeof(swing_6_combo[0]), pressed_keys))
        {
          swing = 6;
        }
        else if (checkCombo(swing_7_combo, sizeof(swing_7_combo) / sizeof(swing_7_combo[0]), pressed_keys))
        {
          swing = 7;
        }
        else if (checkCombo(swing_8_combo, sizeof(swing_8_combo) / sizeof(swing_8_combo[0]), pressed_keys))
        {
          swing = 8;
        }
        else if (checkCombo(swing_9_combo, sizeof(swing_9_combo) / sizeof(swing_9_combo[0]), pressed_keys))
        {
          swing = 9;
        }
      }
    }
    else if (e.bit.EVENT == KEY_JUST_RELEASED)
    {
      Serial.println(" released\n");

      pressed_keys[key] = false;

      if (!combo_pressed)
      {
        if (main_mode)
        {
          main_grid[col][row + row_offset].toggle();                                                   // toggle note
          trellis.setPixelColor(key, main_grid[col][row + row_offset].is_on ? main_color : off_color); // toggle key light
        }
        else
        {
          shift_grid[col][row + row_offset].toggle();                                                    // toggle note
          trellis.setPixelColor(key, shift_grid[col][row + row_offset].is_on ? shift_color : off_color); // toggle key light
        }
      }
      else
      {
        if (numberOfButtonPressed(pressed_keys, sizeof(pressed_keys)) == 0)
        {
          if (main_mode)
          {
            for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
            {
              for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
              {
                trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_color : off_color);
              }
            }
          }
          else
          {
            for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
            {
              for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
              {
                trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_color : off_color);
              }
            }
          }
          combo_pressed = false;
        }
      }
    }
  }

  trellis.sendMIDI(); // send any pending MIDI messages

  delay(10);
}
