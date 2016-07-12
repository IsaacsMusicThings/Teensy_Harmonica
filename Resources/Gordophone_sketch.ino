#define MIDI_CHANNEL 1
// The threshold level for sending a note on event. If the
// sensor is producing a level above this, we should be sounding
// a note.
#define NOTE_ON_THRESHOLD 80
// The maximum raw pressure value you can generate by
// blowing into the tube.
#define MAX_PRESSURE 500

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

// The five notes, from which we choose one at random
unsigned int notes[5] = {60, 62, 65, 67, 69};

// We keep track of which note is sounding, so we know
// which note to turn off when breath stops.
int noteSounding;
// The value read from the sensor
int sensorValue;
// The state of our state machine
int state;
// The time that we noticed the breath off -> on transition
unsigned long breath_on_time = 0L;
// The breath value at the time we observed the transition
int initial_breath_value;
// The aftertouch value we will send
int atVal;
// The last time we sent an aftertouch value
unsigned long atSendTime = 0L;


void setup() {
  state = NOTE_OFF;  // initialize state machine
}

int get_note() {
  return notes[random(0,4)];
}

int get_velocity(int initial, int final, unsigned long time_delta) {
  return map(final, NOTE_ON_THRESHOLD, MAX_PRESSURE, 0, 127);
}

void loop() {
  // read the input on analog pin 0
  sensorValue = analogRead(A0);
  if (state == NOTE_OFF) {
    if (sensorValue > NOTE_ON_THRESHOLD) {
      // Value has risen above threshold. Move to the RISE_TIME
      // state. Record time and initial breath value.
      breath_on_time = millis();
      initial_breath_value = sensorValue;
      state = RISE_TIME;  // Go to next state
    }
  } else if (state == RISE_TIME) {
    if (sensorValue > NOTE_ON_THRESHOLD) {
      // Has enough time passed for us to collect our second
      // sample?
      if (millis() - breath_on_time > RISE_TIME) {
        // Yes, so calculate MIDI note and velocity, then send a note on event
        noteSounding = get_note();
        int velocity = get_velocity(initial_breath_value, sensorValue, RISE_TIME);
        usbMIDI.sendNoteOn(noteSounding, velocity, MIDI_CHANNEL);
        state = NOTE_ON;
      }
    } else {
      // Value fell below threshold before RISE_TIME passed. Return to
      // NOTE_OFF state (e.g. we're ignoring a short blip of breath)
      state = NOTE_OFF;
    }
  } else if (state == NOTE_ON) {
    if (sensorValue < NOTE_ON_THRESHOLD) {
      // Value has fallen below threshold - turn the note off
      usbMIDI.sendNoteOff(noteSounding, 100, MIDI_CHANNEL);  
      state = NOTE_OFF;
    } else {
      // Is it time to send more aftertouch data?
      if (millis() - atSendTime > AT_INTERVAL) {
        // Map the sensor value to the aftertouch range 0-127
        atVal = map(sensorValue, NOTE_ON_THRESHOLD, 1023, 0, 127);
        usbMIDI.sendAfterTouch(atVal, MIDI_CHANNEL);
        atSendTime = millis();
      }
    }
  }
}
