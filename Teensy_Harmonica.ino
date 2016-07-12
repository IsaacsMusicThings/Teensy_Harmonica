#define MIDI_CHANNEL 1
// The threshold level for sending a blow note on event. If the
// sensor is producing a level above this, we should be sounding
// a note.
#define BNOTE_ON_THRESHOLD 520
// The threshold level for sending a draw note on event. If the
// sensor is producing a level below this, we should be sounding
// a note.
#define DNOTE_ON_THRESHOLD 510
// The maximum raw pressure value you can generate by
// blowing into a tube.
#define MAX_PRESSURE 740
// The minimum raw pressure value you can generate by
// drawing on a tube.
#define MAX_PRESSURE 290

// The three states of our state machine
// No note is sounding
#define NOTE_OFF 1
// We've observed a transition from below to above the
// threshold value. We wait a while to see how fast the
// breath velocity is increasing
#define RISE_TIME 10
// A note is sounding
#define NOTE_ON 3
// Send aftertouch data no more than every AT_INTERVAL
// milliseconds
#define AT_INTERVAL 70

// The 14 notes of our harmonica
unsigned int notes[14] = {48, 50, 53, 55, 57, 60, 62, 65, 67, 69, 72, 74, 77, 79};

// We keep track of which note is sounding, so we know
// which note to turn off when breath stops.
int noteSounding0;
int noteSounding1;
int noteSounding2;
int noteSounding3;
int noteSounding4;
int noteSounding5;
int noteSounding6;
// The values read from the 7 sensors
int sensor0Value;
int sensor1Value;
int sensor2Value;
int sensor3Value;
int sensor4Value;
int sensor5Value;
int sensor6Value;
// The states of our 7 state machines
int state0;
int state1;
int state2;
int state3;
int state4;
int state5;
int state6;
// The times that we noticed the breath off -> on transition for each sensor
unsigned long breath_on_time0 = 0L;
unsigned long breath_on_time1 = 0L;
unsigned long breath_on_time2 = 0L;
unsigned long breath_on_time3 = 0L;
unsigned long breath_on_time4 = 0L;
unsigned long breath_on_time5 = 0L;
unsigned long breath_on_time6 = 0L;
// The breath values at the time we observed the transition
int initial_breath_value0;
int initial_breath_value1;
int initial_breath_value2;
int initial_breath_value3;
int initial_breath_value4;
int initial_breath_value5;
int initial_breath_value6;
// The aftertouch value we will send for each note
int atVal0;
int atVal1;
int atVal2;
int atVal3;
int atVal4;
int atVal5;
int atVal6;
// The last time we sent an aftertouch value for each note
unsigned long atSendTime0 = 0L;
unsigned long atSendTime1 = 0L;
unsigned long atSendTime2 = 0L;
unsigned long atSendTime3 = 0L;
unsigned long atSendTime4 = 0L;
unsigned long atSendTime5 = 0L;
unsigned long atSendTime6 = 0L;


void setup() {
  state0 = NOTE_OFF;  // initialize state machine 0
}

int get_note0() {
  return notes[random(0,13)];
}

int get_velocity0(int initial, int final, unsigned long time_delta) {
  return map(final, NOTE_ON_THRESHOLD, MAX_PRESSURE, 0, 127);
}

void loop() {
  // read the input on analog pin 0
  sensorValue0 = analogRead(A0);
  if (state0 == NOTE_OFF) {
    if (sensorValue0 > NOTE_ON_THRESHOLD) {
      // Value has risen above threshold. Move to the RISE_TIME
      // state. Record time and initial breath value.
      breath_on_time0 = millis();
      initial_breath_value0 = sensorValue0;
      state0 = RISE_TIME;  // Go to next state
    }
  } else if (state0 == RISE_TIME) {
    if (sensorValue0 > NOTE_ON_THRESHOLD) {
      // Has enough time passed for us to collect our second
      // sample?
      if (millis() - breath_on_time0 > RISE_TIME) {
        // Yes, so calculate MIDI note and velocity, then send a note on event
        noteSounding0 = get_note0();
        int velocity0 = get_velocity0(initial_breath_value0, sensorValue0, RISE_TIME);
        usbMIDI.sendNoteOn0(noteSounding0, velocity0, MIDI_CHANNEL);
        state = NOTE_ON;
      }
    } else {
      // Value fell below threshold before RISE_TIME passed. Return to
      // NOTE_OFF state (e.g. we're ignoring a short blip of breath)
      state0 = NOTE_OFF;
    }
  } else if (state0 == NOTE_ON) {
    if (sensorValue0 < NOTE_ON_THRESHOLD) {
      // Value has fallen below threshold - turn the note off
      usbMIDI.sendNoteOff0(noteSounding0, 100, MIDI_CHANNEL);  
      state0 = NOTE_OFF;
    } else {
      // Is it time to send more aftertouch data?
      if (millis() - atSendTime0 > AT_INTERVAL) {
        // Map the sensor value to the aftertouch range 0-127
        atVal0 = map(sensorValue0, NOTE_ON_THRESHOLD, 1023, 0, 127);
        usbMIDI.sendAfterTouch(atVal0, MIDI_CHANNEL);
        atSendTime0 = millis();
      }
    }
  }
}
