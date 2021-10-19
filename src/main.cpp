// Trellis M4 MIDI Keypad CC
// sends 32 notes, pitch bend & a CC from accelerometer tilt over USB MIDI

#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <MIDIUSB.h>
#include "Note.h"

#define MIDI_CHANNEL     0  // default channel # is 0
#define FIRST_MIDI_NOTE 36

Adafruit_NeoTrellisM4 trellis = Adafruit_NeoTrellisM4();
Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

uint32_t tick = 0;

// colors
uint32_t press_color = 0XFFFFFF;
uint32_t on_color = 0X00FF00;
uint32_t off_color = 0X0;

const int NUMBER_OF_COLUMNS = 8;
const int NUMBER_OF_ROWS = 4;

Note main_grid[NUMBER_OF_COLUMNS][NUMBER_OF_ROWS];

boolean pressed_keys[32];
boolean combo_pressed = false;

// floating point map
float ofMap(float value, float inputMin, float inputMax, float outputMin, float outputMax, bool clamp) {
    float outVal = ((value - inputMin) / (inputMax - inputMin) * (outputMax - outputMin) + outputMin);

    if (clamp) {
      if (outputMax < outputMin) {
        if (outVal < outputMax)  outVal = outputMax;
        else if (outVal > outputMin)  outVal = outputMin;
      } else {
        if (outVal > outputMax) outVal = outputMax;
        else if (outVal < outputMin)  outVal = outputMin;
      }
    }
    return outVal;
}

void play(Note note)
{
  if (note.is_on) {
    if (note.is_accented) 
    {
      trellis.noteOn(note.midi, 127);
    } else {
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
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return trellis.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return trellis.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return trellis.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

uint32_t sixteenthNoteToTicks(uint8_t sixteenthNote) {
  return sixteenthNote * 6;
}

uint32_t tickToEighthNote(uint32_t tick) {
  return tick / 12;
}

int coordinatesToKey( int col, int row ) {
  return ( row * 8 ) + col;
}

int getGridNote(int start, int rows, int index) {
  return (start + rows - 1) - index;
}

int numberOfButtonPressed( bool pressed_buttons[], int size ) {
  int count = 0;

  for ( int i = 0; i < size; ++i ) {
    if (pressed_buttons[i]) {
      count++;
    }
  }

  return count;
}

void setup(){
  Serial.begin(115200);
    
  trellis.begin();
  trellis.setBrightness(80);

  // USB MIDI messages sent over the micro B USB port
  Serial.println("Enabling MIDI on USB");
  trellis.enableUSBMIDI(true);
  trellis.setUSBMIDIchannel(MIDI_CHANNEL);

  for (int i = 0; i < NUMBER_OF_COLUMNS; i++) {
    for (int j = 0; j < NUMBER_OF_ROWS; j++) {
      main_grid[i][j] = Note();
      main_grid[i][j].set_note(getGridNote(STARTING_MIDI_NOTE, NUMBER_OF_ROWS, j));
      main_grid[i][j].set_key(coordinatesToKey(i, j));
    }
  }
}

void loop() {
  trellis.tick();

  midiEventPacket_t midi_in =  MidiUSB.read();

  if (midi_in.header == 3) { // transport start

    Serial.println("start");
    tick = sixteenthNoteToTicks(midi_in.byte2); // syncs ticks to transport

  } 
  else if (midi_in.header == 11) { // transport end

    Serial.println("stop");

  } 
  else if (midi_in.header == 15) { // tick event - happens 24 times per quarter note

    if ( tick % 12 == 0 ) {
      for ( Note note: main_grid[tickToEighthNote(tick) % 8] ) {
        play(note);
        trellis.setPixelColor(note.key, press_color);
      }
      for ( Note note: main_grid[(tickToEighthNote(tick) - 1) % 8]) {
        stop(note);
        trellis.setPixelColor(note.key, note.is_on ? on_color : off_color);
      }
    }
    tick++;
  }
  
  else if (midi_in.header != 0) {
    Serial.println("message in");
    Serial.println(midi_in.header);
    Serial.println(" ");
    Serial.println(midi_in.byte1);
    Serial.println(" ");
    Serial.println(midi_in.byte2);
    Serial.println(" ");
    Serial.println(midi_in.byte3);
  }

  while (trellis.available()){
    keypadEvent e = trellis.read();
    int key = e.bit.KEY;
    int col = e.bit.COL;
    int row = e.bit.ROW;
    
    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      Serial.println(" pressed\n");
      pressed_keys[key] = true;
      if (numberOfButtonPressed(pressed_keys, sizeof(pressed_keys)) > 2) {
        combo_pressed = true;
      }
    }
    else if (e.bit.EVENT == KEY_JUST_RELEASED) {
      Serial.println(" released\n");

      pressed_keys[key] = false;

      if (!combo_pressed) {
        main_grid[col][row].toggle(); // toggle note
        trellis.setPixelColor(key, main_grid[col][row].is_on ? on_color : off_color); // toggle key light
      } 
      else {
        if (numberOfButtonPressed(pressed_keys, sizeof(pressed_keys)) == 0) {
          combo_pressed = false;
        }
      }
    }
  }

  trellis.sendMIDI(); // send any pending MIDI messages

  delay(10);
}
