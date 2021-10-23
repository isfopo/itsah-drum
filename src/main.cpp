// Trellis M4 MIDI Keypad CC
// sends 32 notes, pitch bend & a CC from accelerometer tilt over USB MIDI

#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <MIDIUSB.h>
#include "Note.h"

#define MIDI_CHANNEL 0 // default channel # is 0
#define FIRST_MIDI_NOTE 36
#define NUMBER_OF_KEYS_ON_TRELLIS 32
#define NUMBER_OF_COLUMNS_ON_TRELLIS 8
#define NUMBER_OF_ROWS_ON_TRELLIS 4

Adafruit_NeoTrellisM4 trellis = Adafruit_NeoTrellisM4();
Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

uint32_t tick = 0;

// colors
uint32_t column_color = 0XEDECEE;
uint32_t main_color = 0X47FFC2;
uint32_t main_accent_color = 0X007A52; 
uint32_t shift_color = 0x9D70FF;
uint32_t shift_accent_color = 0x2D008F;
uint32_t ref_color_1 = 0xFFCA85;
uint32_t ref_color_2 = 0xF694FF;
uint32_t ref_color_3 = 0x82E2FF;
uint32_t ref_color_4 = 0x8E5572;
uint32_t clear_color = 0xFF6767;
uint32_t off_color = 0X0;

const int NUMBER_OF_COLUMNS = 32;
const int NUMBER_OF_ROWS = 16;
const int TICKS_IN_MEASURE = 96;
const int HOLD_TIME = 500;

int row_offset = 12;
int last_step = 8;
int swing = 6;

Note main_grid[NUMBER_OF_COLUMNS][NUMBER_OF_ROWS];
Note shift_grid[NUMBER_OF_COLUMNS][NUMBER_OF_ROWS];

boolean pressed_keys[32];
unsigned long when_key_was_pressed = 0;
boolean combo_pressed = false;
boolean is_upbeat = false;

// modes
boolean main_mode = true;
boolean manual_note_play_mode = false;
boolean manual_note_record_mode = false;
boolean manual_cc_mode = false;

// combos
int show_combo[] = {7, 31};
int back_combo[] = {12, 7, 31};
int shift_combo[] = {4, 7, 31};
int clear_combo[] = {26, 7, 31};
int offset_init_combo[] = {7, 31};
int offset_up_combo[] = {13, 7, 31};
int offset_down_combo[] = {21, 7, 31};
int last_step_left_combo[] = {20, 7, 31};
int last_step_right_combo[] = {22, 7, 31};
int swing_init_combo[] = {7, 31};
int swing_6_combo[] = {27, 7, 31};
int swing_7_combo[] = {19, 7, 31};
int swing_8_combo[] = {11, 7, 31};
int swing_9_combo[] = {3, 7, 31};
int manual_note_play_combo[] = {13, 29};
int manual_note_record_combo[] = {5, 29};
int manual_cc_combo[] = {4, 28};

int manual_cc_channels[] = {
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    85,
    86,
    87,
    88,
    89,
    90,
};

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

uint32_t tickToMeasure(uint32_t tick)
{
  return tick / 96;
}

int coordinatesToKey(int col, int row)
{
  return (row * 8) + col;
}

int getGridNote(int start, int rows, int index)
{
  return (start + rows - 1) - index;
}

boolean isInRangeOfRows(int midi_note)
{
  return ((FIRST_MIDI_NOTE + NUMBER_OF_ROWS) - row_offset - NUMBER_OF_ROWS_ON_TRELLIS) <= midi_note && midi_note < ((FIRST_MIDI_NOTE + NUMBER_OF_ROWS) - row_offset);
}

int getColumnOffset(uint32_t tick)
{
  return ((tick / 96) % (last_step / 8)) * 8;
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

boolean isOnLeftHalfOfTrellis(int key)
{
  return key % NUMBER_OF_COLUMNS_ON_TRELLIS < NUMBER_OF_COLUMNS_ON_TRELLIS / 2;
}

int mapKeyToLeftHalfOfTrellis(int key)
{
  if (key % NUMBER_OF_COLUMNS_ON_TRELLIS == NUMBER_OF_COLUMNS_ON_TRELLIS / 2)
  {
    // https://www.desmos.com/calculator/xy1aphhomd
    if (key < 8)
    {
      return (key * -4) + 15;
    }
    else if (key < 16)
    {
      return (key * -4) + 46;
    }
    else if (key < 24)
    {
      return (key * -4) + 77;
    }
    else if (key < 32)
    {
      return (key * -4) + 108;
    }
  }
}

int mapKeyToRow(int key)
{
  return (mapKeyToLeftHalfOfTrellis(key) * -1) + 15;
}

int getPostitionFromTick(int tick, int last_step, int swing)
{
  return (int)((tick % (last_step * 12) / (float)swing) / 2);
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
      main_grid[i][j].set_key(coordinatesToKey(i % 8, j % 4));
    }
  }
  for (int i = 0; i < NUMBER_OF_COLUMNS; i++)
  {
    for (int j = 0; j < NUMBER_OF_ROWS; j++)
    {
      shift_grid[i][j] = Note();
      shift_grid[i][j].set_note(getGridNote(FIRST_MIDI_NOTE, NUMBER_OF_ROWS, j));
      shift_grid[i][j].set_key(coordinatesToKey(i % 8, j % 4));
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
    if (tick % TICKS_IN_MEASURE == 0)
    {
      if (main_mode)
      {
        for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
        {
          for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
          {
            trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i + getColumnOffset(tick)][j + row_offset].is_on ? main_grid[i + getColumnOffset(tick)][j + row_offset].is_accented ? main_accent_color : main_color : off_color);
          }
        }
      }
      else
      {
        for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
        {
          for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
          {
            trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i + getColumnOffset(tick)][j + row_offset].is_on ? shift_grid[i + getColumnOffset(tick)][j + row_offset].is_accented ? shift_accent_color : shift_color : off_color);
          }
        }
      }
    }
    if (tick % 12 == 0)
    {
      for (Note note : main_grid[tickToEighthNote(tick) % 8])
      {
        if (main_mode)
        {
          trellis.setPixelColor(note.key, column_color);
        }
      }
      for (Note note : main_grid[((tickToEighthNote(tick) - 1) % 8) + getColumnOffset(tick)])
      {
        if (main_mode)
        {
          if (isInRangeOfRows(note.midi))
          {
            trellis.setPixelColor(note.key, note.is_on ? note.is_accented ? main_accent_color : main_color : off_color);
          }
        }
      }
    }
    else if ((int)(tick % 12) == swing - (swing / 2))
    {
      is_upbeat = true;
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
      for (Note note : shift_grid[((tickToEighthNote(tick) - 1) % 8) + getColumnOffset(tick)])
      {
        if (!main_mode)
        {
          if (isInRangeOfRows(note.midi))
          {
            trellis.setPixelColor(note.key, note.is_on ? note.is_accented ? shift_accent_color : shift_color : off_color);
          }
        }
      }
    }
    else if ((int)(tick % 12) == swing + ((12 - swing) / 2))
    {
      is_upbeat = false;
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
    if (checkCombo(show_combo, sizeof(show_combo) / sizeof(show_combo[0]), pressed_keys))
    {
      if (!main_mode)
      {
        trellis.setPixelColor(back_combo[0], ref_color_1);
      }
      else
      {
        trellis.setPixelColor(shift_combo[0], ref_color_1);
      }
      trellis.setPixelColor(clear_combo[0], clear_color);
      trellis.setPixelColor(offset_up_combo[0], ref_color_2);
      trellis.setPixelColor(offset_down_combo[0], ref_color_2);
      trellis.setPixelColor(last_step_left_combo[0], ref_color_3);
      trellis.setPixelColor(last_step_right_combo[0], ref_color_3);
      trellis.setPixelColor(swing_6_combo[0], ref_color_4);
      trellis.setPixelColor(swing_7_combo[0], ref_color_4);
      trellis.setPixelColor(swing_8_combo[0], ref_color_4);
      trellis.setPixelColor(swing_9_combo[0], ref_color_4);
    }
    else if (checkCombo(manual_note_play_combo, sizeof(manual_note_play_combo) / sizeof(manual_note_play_combo[0]), pressed_keys))
    {
      manual_note_play_mode = true;
      for (int i = 0; i < NUMBER_OF_KEYS_ON_TRELLIS; i++)
      {
        if (isOnLeftHalfOfTrellis(i))
        {
          trellis.setPixelColor(i, ref_color_1);
        }
      }
    }
    else if (checkCombo(manual_note_record_combo, sizeof(manual_note_record_combo) / sizeof(manual_note_record_combo[0]), pressed_keys))
    {
      manual_note_record_mode = true;
      for (int i = 0; i < NUMBER_OF_KEYS_ON_TRELLIS; i++)
      {
        if (isOnLeftHalfOfTrellis(i))
        {
          trellis.setPixelColor(i, ref_color_2);
        }
      }
    }
    else if (checkCombo(manual_cc_combo, sizeof(manual_cc_combo) / sizeof(manual_cc_combo[0]), pressed_keys))
    {
      manual_cc_mode = true;
      for (int i = 0; i < NUMBER_OF_KEYS_ON_TRELLIS; i++)
      {
        if (isOnLeftHalfOfTrellis(i))
        {
          trellis.setPixelColor(i, ref_color_3);
        }
      }
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
      Serial.print("key: ");
      Serial.println(key);
      pressed_keys[key] = true;
      when_key_was_pressed = millis();
      if (numberOfButtonPressed(pressed_keys, sizeof(pressed_keys)) > 1)
      {
        combo_pressed = true;
        if (manual_note_play_mode && isOnLeftHalfOfTrellis(key) && checkCombo(manual_note_play_combo, sizeof(manual_note_play_combo) / sizeof(manual_note_play_combo[0]), pressed_keys))
        {
          trellis.noteOn(FIRST_MIDI_NOTE + mapKeyToLeftHalfOfTrellis(key), 96);
        }
        else if (manual_note_record_mode && isOnLeftHalfOfTrellis(key) && checkCombo(manual_note_record_combo, sizeof(manual_note_record_combo) / sizeof(manual_note_record_combo[0]), pressed_keys))
        {
          trellis.noteOn(FIRST_MIDI_NOTE + mapKeyToLeftHalfOfTrellis(key), 96);
          if (!is_upbeat)
          {
            main_grid[getPostitionFromTick(tick, last_step, swing)][mapKeyToRow(key)].on();
          }
          else
          {
            shift_grid[getPostitionFromTick(tick, last_step, swing)][mapKeyToRow(key)].on();
          }
        }
        else if (manual_cc_mode && isOnLeftHalfOfTrellis(key) && checkCombo(manual_cc_combo, sizeof(manual_cc_combo) / sizeof(manual_cc_combo[0]), pressed_keys))
        {
          trellis.controlChange(manual_cc_channels[mapKeyToLeftHalfOfTrellis(key)], 127);
        }
        else if (checkCombo(shift_combo, sizeof(shift_combo) / sizeof(shift_combo[0]), pressed_keys))
        {
          main_mode = false;

          for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
          {
            for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
            {
              trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_grid[i][j + row_offset].is_accented ? shift_accent_color : shift_color : off_color);
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
              trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_grid[i][j + row_offset].is_accented ? main_accent_color : main_color : off_color);
            }
          }
        }
        else if (checkCombo(offset_up_combo, sizeof(offset_up_combo) / sizeof(offset_up_combo[0]), pressed_keys))
        {
          if (row_offset > 0)
          {
            row_offset -= NUMBER_OF_ROWS_ON_TRELLIS;
            if (main_mode)
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_grid[i][j + row_offset].is_accented ? main_accent_color : main_color : off_color);
                }
              }
            }
            else
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_grid[i][j + row_offset].is_accented ? shift_accent_color : shift_color : off_color);
                }
              }
            }
          }
        }
        else if (checkCombo(offset_down_combo, sizeof(offset_down_combo) / sizeof(offset_down_combo[0]), pressed_keys))
        {
          if (row_offset < 12)
          {
            row_offset += NUMBER_OF_ROWS_ON_TRELLIS;
            if (main_mode)
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i][j + row_offset].is_on ? main_grid[i][j + row_offset].is_accented ? main_accent_color : main_color : off_color);
                }
              }
            }
            else
            {
              for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
              {
                for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
                {
                  trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i][j + row_offset].is_on ? shift_grid[i][j + row_offset].is_accented ? shift_accent_color : shift_color : off_color);
                }
              }
            }
          }
        }
        else if (checkCombo(last_step_left_combo, sizeof(last_step_left_combo) / sizeof(last_step_left_combo[0]), pressed_keys))
        {
          if (last_step > NUMBER_OF_COLUMNS_ON_TRELLIS)
          {
            last_step -= NUMBER_OF_COLUMNS_ON_TRELLIS;
          }
        }
        else if (checkCombo(last_step_right_combo, sizeof(last_step_right_combo) / sizeof(last_step_right_combo[0]), pressed_keys))
        {
          if (last_step < NUMBER_OF_COLUMNS)
          {
            last_step += NUMBER_OF_COLUMNS_ON_TRELLIS;
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
        else if (checkCombo(clear_combo, sizeof(clear_combo) / sizeof(clear_combo[0]), pressed_keys))
        {
          for (int i = 0; i < NUMBER_OF_COLUMNS; i++)
          {
            for (int j = 0; j < NUMBER_OF_ROWS; j++)
            {
              main_grid[i][j].off();
            }
          }
          for (int i = 0; i < NUMBER_OF_COLUMNS; i++)
          {
            for (int j = 0; j < NUMBER_OF_ROWS; j++)
            {
              shift_grid[i][j].off();
            }
          }
        }
      }
    }
    else if (e.bit.EVENT == KEY_JUST_RELEASED)
    {
      pressed_keys[key] = false;
      if (!combo_pressed)
      {
        if (main_mode)
        {
          if (millis() - when_key_was_pressed < HOLD_TIME)
          {
            main_grid[col + getColumnOffset(tick)][row + row_offset].toggle();
            trellis.setPixelColor(key, main_grid[col][row + row_offset].is_on ? main_color : off_color);
          }
          else
          {
            main_grid[col + getColumnOffset(tick)][row + row_offset].toggle_accent();
            trellis.setPixelColor(key, main_grid[col][row + row_offset].is_accented ? main_accent_color : off_color);
          }
        }
        else
        {
          if (millis() - when_key_was_pressed < HOLD_TIME)
          {
            shift_grid[col + getColumnOffset(tick)][row + row_offset].toggle();
            trellis.setPixelColor(key, shift_grid[col][row + row_offset].is_on ? shift_color : off_color);
          }
          else
          {
            shift_grid[col + getColumnOffset(tick)][row + row_offset].toggle_accent();
            trellis.setPixelColor(key, main_grid[col][row + row_offset].is_accented ? shift_accent_color : off_color);
          }
        }

        if (manual_note_play_mode)
        {
          manual_note_play_mode = false;
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
                trellis.setPixelColor(coordinatesToKey(i, j), main_grid[i + getColumnOffset(tick)][j + row_offset].is_on ? main_grid[i][j + row_offset].is_accented ? main_accent_color : main_color : off_color);
              }
            }
          }
          else
          {
            for (int i = 0; i < NUMBER_OF_COLUMNS_ON_TRELLIS; i++)
            {
              for (int j = 0; j < NUMBER_OF_ROWS_ON_TRELLIS; j++)
              {
                trellis.setPixelColor(coordinatesToKey(i, j), shift_grid[i + getColumnOffset(tick)][j + row_offset].is_on ? shift_grid[i][j + row_offset].is_accented ? shift_accent_color : shift_color : off_color);
              }
            }
          }
          combo_pressed = false;
        }
        else if (manual_note_play_mode && isOnLeftHalfOfTrellis(key))
        {
          trellis.noteOff(FIRST_MIDI_NOTE + mapKeyToLeftHalfOfTrellis(key), 0);
        }
        else if (manual_note_record_mode && isOnLeftHalfOfTrellis(key))
        {
          trellis.noteOff(FIRST_MIDI_NOTE + mapKeyToLeftHalfOfTrellis(key), 0);
        }
        else if (manual_cc_mode && isOnLeftHalfOfTrellis(key))
        {
          trellis.controlChange(manual_cc_channels[mapKeyToLeftHalfOfTrellis(key)], 0);
        }
      }
    }
  }

  trellis.sendMIDI(); // send any pending MIDI messages

  delay(1);
}
