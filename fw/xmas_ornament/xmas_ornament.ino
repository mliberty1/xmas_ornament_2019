
/* 
 * Liberty family Christmas ornament 2019
 * Arduino software for the SAMD21 (like Adafruit Feather M0 basic)
 */

/*
Instructions:

You can animate each LED using the asychronous commands:
   * led_set: set the LED intensity.
   * led_fade: smoothly transition from current intensity to a new intensity
   * led_delay: wait at the current intensity
   * led_signal: signal a function, which can program more commands.

The animation for each LED runs in parallel and independently.
However, you can chain together animations using the signal
callback.
*/

#include "led.h"
#include "button.h"

const uint8_t leds[LED_COUNT] = {LED_10, LED_09, LED_08, LED_07, LED_06, LED_05, LED_02, LED_03, LED_04, LED_01};
const int ANIMATE_RANDOM_DURATION_MS = 500;

int mode = 0;
int state = 0;

void mode_set_all(void * user_data) {
  leds_write_all((int) user_data);
}

void mode_animate_sequential(void * user_data) {
  int led = state;
  led_fade(led, 255, 300);
  led_signal(led, mode_animate_sequential, 0);
  led_delay(led, 300);
  led_fade(led, 0, 300);
  ++state;
  if (state >= LED_COUNT) {
    state = 0;
  }
}

void mode_animate_random(void * user_data) {
  int led = led_random_from_idle();
  led_fade(led, 255, ANIMATE_RANDOM_DURATION_MS);
  led_signal(led, mode_animate_random, 0);
  led_delay(led, ANIMATE_RANDOM_DURATION_MS);
  led_fade(led, 0, ANIMATE_RANDOM_DURATION_MS);
}

void mode_animate_sequential_seizure(void * user_data) {
  int led = state;
  led_fade(led, 255, 5);
  led_delay(led, 10);
  led_fade(led, 0, 0);
  led_signal(led, mode_animate_sequential_seizure, 0);
  ++state;
  if (state >= LED_COUNT) {
    state = 0;
  }
}

void mode_wait_for_button(void * user_data) {
  // todo enter low power state
}

struct mode_s {
  void * user_data;
  callback_fn initialize;
  callback_fn process;
};

// Define the modes: each button press switches to the next mode.
// Modify this structure to add your mode!
struct mode_s modes[] = {
  {0, mode_animate_random, led_animate},
  {0, mode_animate_sequential_seizure, led_animate},
  {0, mode_animate_sequential, led_animate},
  {(void *) 0, mode_set_all, mode_wait_for_button}  // off
};


// ----------------------------------------------------------------
// Animation engine: do not modify code below
// ----------------------------------------------------------------

void setup() {
  analogWriteResolution(ADC_BITS);
  led_initialize();

  Serial.begin(9600);
  Serial.println("\n\nLiberty christmas ornament 2019");
  for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
    pinMode(leds[i], OUTPUT);
  }
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(POWER_USB, INPUT);
  if (digitalRead(POWER_USB)) {
    Serial.println("powered over USB");
  } else {
    Serial.println("powered by battery");
  }
  if (modes[mode].initialize) {
    modes[mode].initialize(modes[mode].user_data);
  }
}

void loop() {
  if (button_debounce()) {
      Serial.println("button pressed");
      led_clear();
      ++mode;
      if (mode >= ARRAY_SIZE(modes)) {
        mode = 0;
      }
      if (modes[mode].initialize) {
        state = 0;
        modes[mode].initialize(modes[mode].user_data);
      }
  }

  if (modes[mode].process) {
    modes[mode].process(modes[mode].user_data);
  }

  // todo enter low power automatically when on battery.
}
