/*
 * Example Arduino nRF24L01+ transmitter firmware compatible with the ESPHome
 * `nrf24` component running in RX mode.
 *
 * This is a reference sketch (as flashed on a real remote: an Adafruit Feather
 * M0 Express with four buttons, a NeoPixel and deep-sleep between presses). On
 * each button press it wakes up, transmits a short "button_<n>" message over the
 * nRF24L01+ and goes back to sleep. An ESPHome node using this repository's
 * `nrf24` component with a matching `rx_address` / `rx_pipe` will receive it via
 * its `on_receive:` trigger.
 *
 * Wire compatibility with the component:
 *   - PIPE_ADDRESS below must match the component's `rx_address` (and the pipe
 *     it is opened on, `rx_pipe`).
 *   - Messages are fixed-size 32-byte, zero-padded frames (nRF24L01+ hardware
 *     requirement), which is exactly what the component expects.
 *   - Channel and data rate must match on both ends (the component defaults to
 *     channel 76, 1MBPS). Adjust radio.setChannel()/setDataRate() if you change
 *     the component's `channel` / `data_rate`.
 *
 * Required Arduino libraries: RF24, FastLED, ArduinoLowPower (the latter two are
 * specific to this Feather M0 remote; drop them if your hardware differs).
 *
 * Copyright 2024-2026 KuraLabs S.R.L
 * Licensed under the Apache License, Version 2.0 (see LICENSE at the repo root).
 */

#include <ArduinoLowPower.h>
#include <SPI.h>
#include <RF24.h>
#include <FastLED.h>

// NOTE TO FUTURE SELF:
// The Adafruit Feather M0 Express, when reaching deep sleep, needs to
// be put manually in upload/programming mode by pressing twice the reset button
// until the built in led breaths. Then upload will work.


// User interaction pins
#define BUTTON_1 A1
#define BUTTON_2 A2
#define BUTTON_3 A4
#define BUTTON_4 A5
#define DEBOUNCE_TIME 200  // ms

// Pixel configuration
#define PIXEL 8

CRGB leds[1];

// NRF24 configuration
#define CE_PIN 6
#define CSN_PIN 5
#define MAX_PAYLOAD 32

// Must match the ESPHome component's `rx_address` (and `rx_pipe`).
const uint8_t PIPE_ADDRESS[] = { 0xDE, 0xAD, 0xC0, 0xDE, 0x01 };

RF24 radio(CE_PIN, CSN_PIN);


volatile uint8_t button_pressed = 0;


void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  while (!Serial);  // Wait for Serial to initialize
  Serial.println("Initializing ...");
  Serial.println("[READY] Serial");

  // Initialize pixel
  FastLED.addLeds<NEOPIXEL, PIXEL>(leds, 1);
  FastLED.clear();
  FastLED.show();
  Serial.println("[READY] Pixel");

  // Set buttons as INPUT_PULLUP to enable internal pull-ups
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);
  Serial.println("[READY] I/O");

  // Initialize NRF24 radio
  if (!radio.begin()) {
    while (true) {
      Serial.println("Error: NRF24 Initialization Failed!");
      delay(10000);
    }
  }

  radio.openWritingPipe(PIPE_ADDRESS);  // Set NRF24 address
  radio.setPALevel(RF24_PA_HIGH);       // Set power level
  radio.stopListening();                // Set NRF24 to transmit mode
  radio.powerDown();                    // Low power mode, will wake up when needed
  Serial.println("[READY] Radio");

  // Attach interrupts for waking up the microcontroller
  LowPower.attachInterruptWakeup(BUTTON_1, isr_button_1, FALLING);
  LowPower.attachInterruptWakeup(BUTTON_2, isr_button_2, FALLING);
  LowPower.attachInterruptWakeup(BUTTON_3, isr_button_3, FALLING);
  LowPower.attachInterruptWakeup(BUTTON_4, isr_button_4, FALLING);
  Serial.println("[READY] ISR");

  Serial.println("[DONE]");
}


void loop() {
  // Process button presses
  if (button_pressed > 0) {
    // Cache the button that triggered the wake
    uint8_t handling_button = button_pressed;

    // Notify user of button pressed
    leds[0] = CRGB::Blue;
    FastLED.show();

    // Debounce
    delay(DEBOUNCE_TIME);

    // Reset button identifier after debouncing
    button_pressed = 0;

    // Debug button pressed
    Serial.print("Button pressed: ");
    Serial.println(handling_button);
    Serial.flush();

    // Send message
    if (send(handling_button)) {
      Serial.println("Message sent successfully!");
      leds[0] = CRGB::Green;
    } else {
      Serial.println("Error: Message Sending Failed!");
      leds[0] = CRGB::Red;
    }
    FastLED.show();

    // Notify user button has being handled
    delay(1000);
    FastLED.clear();
    FastLED.show();
  }

  // Put the microcontroller in low power mode
  LowPower.sleep();
}


bool send(uint8_t button) {

  // Prepare memory
  char message[MAX_PAYLOAD];
  memset(message, 0, sizeof(message));  // Zeroize the buffer to avoid dirty memory

  // Format message
  snprintf(message, sizeof(message), "button_%u", button);
  Serial.print("Sending: ");
  Serial.println(message);
  Serial.flush();

  // Wake up
  radio.powerUp();
  delay(5);  // Ensure NRF24 is fully awake

  // Send the message
  bool result = radio.write(&message, sizeof(message));

  // Put NRF24 back to low power
  radio.powerDown();

  return result;
}


// Interrupt Service Routines (ISR) for button presses
void isr_button_1() {
  button_pressed = 1;
}

void isr_button_2() {
  button_pressed = 2;
}

void isr_button_3() {
  button_pressed = 3;
}

void isr_button_4() {
  button_pressed = 4;
}
