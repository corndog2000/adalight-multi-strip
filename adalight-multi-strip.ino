#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// --- General Settings
const uint16_t NUM_LEDS_PER_STRIP = 36;  // LEDs per strip
const uint8_t NUM_STRIPS = 8;            // Number of strips
const uint16_t TOTAL_LEDS = NUM_LEDS_PER_STRIP * NUM_STRIPS;
const uint8_t BRIGHTNESS = 25;           // maximum brightness

// --- NeoPixel Settings
#define COLOR_ORDER NEO_GRB  // NeoPixel color format
#define NEO_TYPE (COLOR_ORDER + NEO_KHZ800)

// --- Pin Settings
const uint8_t PINS[NUM_STRIPS] = {15, 14, 13, 12, 11, 10, 9, 8};  // GPIO pins for each strip

// --- Serial Settings
const unsigned long SERIAL_SPEED = 115200;  // serial port speed
const uint16_t SERIAL_TIMEOUT = 60;  // time before LEDs are shut off if no data (in seconds), 0 to disable

// --- Adalight Protocol Settings
const uint8_t magic[] = {'A','d','a'};
#define MAGICSIZE  sizeof(magic)
#define HICHECK    (MAGICSIZE)
#define LOCHECK    (MAGICSIZE + 1)
#define CHECKSUM   (MAGICSIZE + 2)

// --- Strip Initialization
Adafruit_NeoPixel strips[NUM_STRIPS] = {
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[0], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[1], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[2], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[3], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[4], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[5], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[6], NEO_TYPE),
  Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, PINS[7], NEO_TYPE)
};

enum processModes_t {Header, Data} mode = Header;

int16_t c;  // current byte, must support -1 if no data available
uint16_t outPos;  // current byte index in the LED array
uint32_t bytesRemaining;  // count of bytes yet received, set by checksum
unsigned long t, lastByteTime, lastAckTime;  // millisecond timestamps

void headerMode();
void dataMode();
void timeouts();
void setLED(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
void showAll();

void setup() {
  for (int i = 0; i < NUM_STRIPS; i++) {
    strips[i].begin();
    strips[i].setBrightness(BRIGHTNESS);
    strips[i].show();
  }

  Serial.begin(SERIAL_SPEED);
  Serial.print("Ada\n"); // Send ACK string to host

  lastByteTime = lastAckTime = millis(); // Set initial counters
}

void loop() {
  t = millis(); // Save current time

  // If there is new serial data
  if ((c = Serial.read()) >= 0) {
    lastByteTime = lastAckTime = t; // Reset timeout counters

    switch(mode) {
      case Header:
        headerMode();
        break;
      case Data:
        dataMode();
        break;
    }
  }
  else {
    // No new data
    timeouts();
  }
}

void headerMode() {
  static uint8_t
    headPos,
    hi, lo, chk;

  if (headPos < MAGICSIZE) {
    // Check if magic word matches
    if (c == magic[headPos]) {headPos++;}
    else {headPos = 0;}
  }
  else {
    // Magic word matches! Now verify checksum
    switch(headPos) {
      case HICHECK:
        hi = c;
        headPos++;
        break;
      case LOCHECK:
        lo = c;
        headPos++;
        break;
      case CHECKSUM:
        chk = c;
        if (chk == (hi ^ lo ^ 0x55)) {
          // Checksum looks valid. Get 16-bit LED count, add 1
          // (# LEDs is always > 0) and multiply by 3 for R,G,B.
          bytesRemaining = 3L * (256L * (long)hi + (long)lo + 1L);
          outPos = 0;
          for (int i = 0; i < NUM_STRIPS; i++) {
            strips[i].clear();
          }
          mode = Data; // Proceed to data mode
        }
        headPos = 0; // Reset header position regardless of checksum result
        break;
    }
  }
}

void dataMode() {
  static uint8_t rgb[3];
  
  // If LED data is not full
  if (outPos < TOTAL_LEDS * 3) {
    rgb[outPos % 3] = c;
    if ((outPos % 3) == 2) {
      setLED(outPos / 3, rgb[0], rgb[1], rgb[2]);
    }
    outPos++;
  }
  bytesRemaining--;
 
  if (bytesRemaining == 0) {
    // End of data -- show LEDs:
    mode = Header; // Begin next header search
    showAll();
    while (Serial.available() > 0) { Serial.read(); } // Clear any remaining data
  }
}

void timeouts() {
  // No data received. If this persists, send an ACK packet
  // to host once every second to alert it to our presence.
  if ((t - lastAckTime) >= 1000) {
    Serial.print("Ada\n"); // Send ACK string to host
    lastAckTime = t; // Reset counter

    // If no data received for an extended time, turn off all LEDs.
    if (SERIAL_TIMEOUT != 0 && (t - lastByteTime) >= (uint32_t) SERIAL_TIMEOUT * 1000) {
      for (int i = 0; i < NUM_STRIPS; i++) {
        strips[i].clear();
        strips[i].show();
      }
      mode = Header;
      lastByteTime = t; // Reset counter
    }
  }
}

void setLED(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t stripIndex = n / NUM_LEDS_PER_STRIP;
  uint16_t ledIndex = n % NUM_LEDS_PER_STRIP;
  
  if (stripIndex < NUM_STRIPS) {
    strips[stripIndex].setPixelColor(ledIndex, r, g, b);
  }
}

void showAll() {
  for (int i = 0; i < NUM_STRIPS; i++) {
    strips[i].show();
  }
}
