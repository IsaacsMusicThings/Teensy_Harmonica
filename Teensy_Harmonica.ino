#define MIDI_CHANNEL 1
// The threshold margin is added to the calculated initial max/min reading
// on a sensor during initialization. Lower value makes the harmonica more 
// sensitive but may also cause false notes if too low. HIGH is added to
// initial note on while LOW is added to sustain the played note.
#define THRESHOLD_HIGH_MARGIN 5
#define THRESHOLD_LOW_MARGIN 2
// The threshold level for sending a draw note on event. If the
// sensor is producing a level below this, we should be sounding
// a note.
#define MAX_PRESSURE 740
// The minimum raw pressure value you can generate by
// drawing on a tube.
#define MIN_PRESSURE 290

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
// how many times read each sensor to find the min/max values
#define INITIALIZATION_COUNT 1000
// for averaged reads, how many samples are needed
#define SAMPLE_COUNT 10
// trhow out high and low values for averaged reads?
#define THROW_OUT_HIGH_AND_LOW true

#define BLOW 0
#define DRAW 1

// The 7 sensor's mapping to analog pins 
int sensors[7] = {A0, A1, A2, A3, A4, A5, A6};

// The 14 notes of our harmonica
unsigned int notes[14] = {48, 50, 53, 55, 57, 60, 62, 65, 67, 69, 72, 74, 77, 79};

// each sensor's min and max when initialized
int note_on_threshold[7][2];
// each sensor's max value possible for BLOW and DRAW
int note_on_max[2] = {MAX_PRESSURE, MIN_PRESSURE};

// We keep track of which note is sounding, so we know
// which note to turn off when breath stops.
int noteSounding[7][2];
// The values read from the 7 sensors
int sensorValue[7];
// The states of our 7 state machines
int state[7][2];
// The times that we noticed the breath off -> on transition for each sensor
unsigned long breath_on_time[7][2];
// The breath values at the time we observed the transition
int initial_breath_value[7];
// The last time we sent an aftertouch value for each note
unsigned long atSendTime[7] = {0L,0L,0L,0L,0L,0L,0L};

int averaged_sensor_read( int sensor ) {
  static int samples = SAMPLE_COUNT;
  int i, values[samples], min=9999, max=0, sum=0, count=0;

  if ( THROW_OUT_HIGH_AND_LOW ) {
    for ( i = 0; i < samples; i++ ) {
      values[i] = analogRead(sensor);
      if ( values[i] > max ) {
        max = values[i];
      } else if ( values[i] < min ) {
        min = values[i];
      }
    }
  
    for ( i = 0; i < samples; i++ ) {
      if ( values[i] == max || values[i] == min ) {
        // skip
      } else {
        sum += values[i];
        count++;
      }
    }
  
    if ( count == 0 ) {
      return (min + max) / 2;
    } else {
      return sum / count;
    }
  } else {
    for ( i = 0; i < samples; i++ ) {
      sum += analogRead(sensor);
    }
    return sum / samples;
  }
}

void reset_thresholds() {
  int sensor_value, sensor_low, sensor_high;

  // find the zero point for all analog sensors
  for (int i=0; i<7; i++) {
    sensor_low = 1024;
    sensor_high = 0;
    for (int j =1; j < INITIALIZATION_COUNT; j++) {
      sensor_value = averaged_sensor_read(sensors[i]);
      // sensor_value = analogRead(sensors[i]);

      if (sensor_value < sensor_low) {
        sensor_low = sensor_value;
      }
      if (sensor_value > sensor_high) {
        sensor_high = sensor_value;
      }
    }
    note_on_threshold[i][BLOW] = sensor_high;
    note_on_threshold[i][DRAW] = sensor_low;

    // initialize states
    state[i][BLOW] = NOTE_OFF;
    state[i][DRAW] = NOTE_OFF;
  }  
}

int get_note(int sensor, int direction) {
  return notes[(sensor*2)+direction];
}

int get_velocity(int sensor, int direction, int final) {
  return map(final, note_on_threshold[sensor][direction], note_on_max[direction], 0, 127);
}

void setup() {
  reset_thresholds();
}

void loop() {
  int i, j;

  // read the input on the analog pins
  for (i=0; i<7; i++)
  {
    // test for BLOW(0) and DRAW(1)
    for (j=0; j<2; j++)
    {
      sensorValue[i] = averaged_sensor_read(sensors[i]);
      // sensorValue[i] = analogRead(sensors[i]);
      if (state[i][j] == NOTE_OFF) {
        if ((j == BLOW && sensorValue[i] > (note_on_threshold[i][j] + THRESHOLD_HIGH_MARGIN))
          || (j == DRAW && sensorValue[i] < (note_on_threshold[i][j] - THRESHOLD_HIGH_MARGIN))) {
          // Value has risen above threshold. Move to the RISE_TIME
          // state. Record time and initial breath value.
          breath_on_time[i][j] = millis();
          initial_breath_value[i] = sensorValue[i];
          state[i][j] = RISE_TIME;  // Go to next state
        }
      } else if (state[i][j] == RISE_TIME) {
        if ((j == BLOW && sensorValue[i] > (note_on_threshold[i][j] + THRESHOLD_HIGH_MARGIN))
          || (j == DRAW && sensorValue[i] < (note_on_threshold[i][j] - THRESHOLD_HIGH_MARGIN))) {
          // Has enough time passed for us to collect our second
          // sample?
          if (millis() - breath_on_time[i][j] > RISE_TIME) {
            // Yes, so calculate MIDI note and velocity, then send a note on event
            noteSounding[i][j] = get_note(i, j);
            int velocity = get_velocity(i, j, sensorValue[i]);
            usbMIDI.sendNoteOn(noteSounding[i][j], velocity, MIDI_CHANNEL);
            state[i][j]= NOTE_ON;
            atSendTime[i] = millis();
          }
        } else {
          // Value fell below threshold before RISE_TIME passed. Return to
          // NOTE_OFF state (e.g. we're ignoring a short blip of breath)
          state[i][j] = NOTE_OFF;
        }
      } else if (state[i][j] == NOTE_ON) {
        if ((j == BLOW && sensorValue[i] > (note_on_threshold[i][j] + THRESHOLD_LOW_MARGIN))
          || (j == DRAW && sensorValue[i] < (note_on_threshold[i][j] - THRESHOLD_LOW_MARGIN))) {
          // Is it time to send more aftertouch data?
          if (millis() - atSendTime[i] > AT_INTERVAL) {
            // Map the sensor value to the aftertouch range 0-127
            // int aftertouch = map(sensorValue[i], NOTE_ON_THRESHOLD, 1023, 0, 127);
            int aftertouch = get_velocity(i, j, sensorValue[i]);
            usbMIDI.sendAfterTouch(aftertouch, MIDI_CHANNEL);
            atSendTime[i] = millis();
          }
        } else {
          // Value has fallen below threshold - turn the note off
          usbMIDI.sendNoteOff(noteSounding[i][j], 100, MIDI_CHANNEL);  
          state[i][j] = NOTE_OFF;
        }
      }
    }
  }
}
