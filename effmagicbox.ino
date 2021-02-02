#include <SoftwareSerial.h>

// number of supported indicators
const int max_indicators = 8;

//
// input indicators
//

// input indicator/pin association
const int input_pins[max_indicators] = {
  A2, // left headlight
  A3, // right headlight
  A0, // left turn
  A1, // right turn
  A5, // reverse gear
  12, // rear fog light
  A4, // brake light
  11, // battery
};

// input indicator activation status (HIGH/LOW)
int input_levels[max_indicators];

//
// output indicators
//

// output indicator/pin association
const int output_pins[max_indicators] = {
   6, // left headlight
   7, // right headlight
   4, // left turn
   5, // right turn
   9, // reverse gear
  10, // rear fog light
   8, // brake light
  -1, // [not used]
};

// output indicator activation status (HIGH/LOW)
int output_levels[max_indicators];

//
// program mode
//
const int mode_pin   = 13;
const int mode_read  = LOW;
const int mode_write = HIGH;

int mode = mode_read;
int old_mode = mode;

//
// Bluetooth HC05 module support
//
const int tx_pin   = 2;  // pin connected to RXD
const int rx_pin   = 3;  // pin connected to TXD

SoftwareSerial hc05_serial(rx_pin, tx_pin);

void setup() {
  // set pin mode as input as soon as possible
  pinMode(mode_pin, INPUT);

  // reset indicator levels
  for (int i = 0; i < max_indicators; i++) {
    input_levels[i] = LOW;
    output_levels[i] = LOW;
  }

  // open HC-05 serial
  hc05_serial.begin(9600);
}

/*
 * Read and execute a BT command.
 * 
 * A command always ends with a '.' character.
 * Currently supported commands are:
 * 
 * M.
 * Requests the current mode.
 * Possible responses are 'W' (write) and 'R' (read).
 * 
 * R.
 * Requests the current state of the indicators.
 * The response is a sequence of 8 'H'/'L' characters.
 * The order is the same defined for the `pins` array.
 * 
 * Wxxxxxxxx.
 * Sets the indicator status. x is one of 'H'/'L'.
 * The order is the same defined for the `pins` array.
 * The response is 'OK'.
 */
void exec_bt_command() {
  static const int buffer_size = 32;
  static char buffer[buffer_size];
  static int pos = 0;
  static bool end_of_command = false;

  while (hc05_serial.available() && !end_of_command) {
    int data = hc05_serial.read();
    buffer[pos] = data;
    pos = (pos + 1) % buffer_size;
    if (end_of_command = (data == '.'))
      break;
  }

  if (end_of_command) {
    pos = 0;
    end_of_command = false;
    
    if (buffer[0] == 'M')
      hc05_serial.print(mode == mode_write ? 'W' : 'R');

    else if (buffer[0] == 'R') {
      if (mode == mode_write)
        hc05_serial.print("ERR");
      else {
        for (int i = 0; i < max_indicators; i++)
          // warning: readings are active low!
          hc05_serial.print(input_levels[i] == HIGH ? 'L' : 'H');
      }
    }

    else if (buffer[0] == 'W') {
      if (mode == mode_read)
        hc05_serial.print("ERR");
      else {
        int i = 0;
        while (i < max_indicators && buffer[i+1] != '.') {
          output_levels[i] = buffer[i+1] == 'H' ? HIGH : LOW;
          ++i;
        }
        hc05_serial.print("OK");
      }
    }
  }
}

void probe_lines() {
  for (int i = 0; i < max_indicators; i++) {
    int pin = input_pins[i];
    if (pin > 0)
      input_levels[i] = digitalRead(pin);
  }
}

void update_lines() {
  for (int i = 0; i < max_indicators; i++) {
    int pin = output_pins[i];
    if (pin > 0)
      digitalWrite(pin, output_levels[i]);
  }
}

void loop() {
  // set operation mode
  mode = digitalRead(mode_pin);

  // reconfigure the pins if the mode changed now
  // (some pins may be used for both input and output)
  if (mode != old_mode) {
    old_mode = mode;
    if (mode == mode_write) {
      for (int i = 0; i < max_indicators; i++)
        pinMode(output_pins[i], OUTPUT);
    } else if (mode == mode_read) {
      for (int i = 0; i < max_indicators; i++)
        pinMode(input_pins[i], INPUT);
    }
  }

  if (mode == mode_read)
    probe_lines();
  else if (mode == mode_write)
    update_lines();

  // execute any command received via Bluetooth
  exec_bt_command();
}
