
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

//https://github.com/adafruit/Adafruit_ZeroTimer/
// https://www.avdweb.nl/arduino/adc-dac/samd21-pwm-dac

#include "led.h"
#include "button.h"
#include <ArduinoLowPower.h>

#define PIN_BATTERY_N A3
const uint8_t leds[LED_COUNT] = {LED_10, LED_09, LED_08, LED_07, LED_06, LED_05, LED_02, LED_03, LED_04, LED_01};
const int ANIMATE_RANDOM_DURATION_MS = 500;
const int ANIMATE_SEQUENTIAL_DURATION_MS = 300;
const int ANIMATE_DAD_MS = 500;
const int ALTERNATE_FADE_MS = 500;
const int ALTERNATE_ON_MS = 500;
const int RUN_DURATION_MINUTES = 20;  // ~20 mA, ~620 mAh
const int RUN_DURATION_MS = RUN_DURATION_MINUTES * 60000;

int mode = 0;
int state = 0;
int time_start = 0;

void mode_set_all(void * user_data) {
  leds_write_all((int) user_data);
}

void mode_animate_down(void * user_data) {
  int led1 = 3 - state;
  int led2 = 6 + state;
  led_fade(led1, 255, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_signal(led1, mode_animate_down, 0);
  led_delay(led1, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_fade(led1, 0, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_fade(led2, 255, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_delay(led2, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_fade(led2, 0, ANIMATE_SEQUENTIAL_DURATION_MS);
  ++state;
  if (state >= 4) {
    state = 0;
  }
  led_fade(4, 191 + random(64), ANIMATE_SEQUENTIAL_DURATION_MS);
}

void mode_down_alternate(void * user_data) {
  int led;
  if (state & 1) {
    led = 3 - (state >> 1);
  } else {
    led = 5 + (state >> 1);
  }
  led_fade(led, 255, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_signal(led, mode_down_alternate, 0);
  led_delay(led, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_fade(led, 0, ANIMATE_SEQUENTIAL_DURATION_MS);
  
  ++state;
  if (state > 8) {
    state = 0;
  }
  led_fade(4, 191 + random(64), ANIMATE_SEQUENTIAL_DURATION_MS);
}

void mode_animate_sequential(void * user_data) {
  int led = state;
  led_fade(led, 255, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_signal(led, mode_animate_sequential, 0);
  led_delay(led, ANIMATE_SEQUENTIAL_DURATION_MS);
  led_fade(led, 0, ANIMATE_SEQUENTIAL_DURATION_MS);
  ++state;
  if (state >= LED_COUNT) {
    state = 0;
  }
}

void mode_gpa_fading(void * user_data) {
  for (int i = 0; i < 4; ++i) {
    for (int led = 0; led < LED_COUNT; ++led) {
      led_fade(led, 255, 500);
      led_delay(led, 500);
      led_fade(led, 0, 500);
      led_delay(led, 100);
    }
  }
  led_delay(4, 500);
  led_signal(4, mode_gpa, 0);
}

void mode_gpa_twinkle_star(void * user_data) {
  for (int i = 0; i < 3000; i += 100) {
    led_fade(4, 128 + random(128), 100);
  }
  led_signal(4, mode_gpa_fading, 0);
}

void mode_gpa(void * user_data) {
  led_fade(0, 255, 0);
  led_fade(9, 255, 0);
  for (int i = 0; i < 8; ++i) {
    int led = i;
    if (0 == (led & 1)) {
      led = 8 - (led >> 1);
    } else {
      led = 1 + (led >> 1);
    }
    led_delay(led, ANIMATE_DAD_MS + i * ANIMATE_DAD_MS);
    led_fade(led, 255, 0);
  }
  led_delay(4, ANIMATE_DAD_MS);
  led_signal(4, mode_gpa_twinkle_star, 0);
}

void mode_star_random(void * user_data) {
  int led = 4;
  while (led == 4) {
    led = led_random_from_idle();
  }
  led_fade(4, 191 + random(64), ANIMATE_RANDOM_DURATION_MS);
  led_fade(led, 255, ANIMATE_RANDOM_DURATION_MS);
  led_signal(led, mode_star_random, 0);
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

void mode_alternate(void * user_data) {
  for (int led = state; led < 10; led += 2) {
    led_fade(led, 255, ALTERNATE_FADE_MS);
    led_delay(led, ALTERNATE_ON_MS);
    if (led == state) {
      led_signal(led, mode_alternate, 0);
    }
    led_fade(led, 0, ALTERNATE_FADE_MS);
  }
  state = (state + 1) & 1;
}

void mode_wait_for_button(void * user_data) {
  mode = 0;
  on_sleep();
}

struct mode_s {
  void * user_data;
  callback_fn initialize;
  callback_fn process;
};

// Define the modes: each button press switches to the next mode.
// Modify this structure to add your mode!
struct mode_s modes[] = {
  {0, mode_star_random, led_animate},
  {0, mode_animate_sequential_seizure, led_animate},
  {0, mode_animate_sequential, led_animate},
  {0, mode_animate_down, led_animate},
  {0, mode_down_alternate, led_animate},
  {0, mode_alternate, led_animate},
  {0, mode_gpa, led_animate},
  {(void *) 0, mode_set_all, mode_wait_for_button}  // off
};

// ----------------------------------------------------------------
// Animation engine: do not modify code below
// ----------------------------------------------------------------

void on_sleep() {
  led_clear();
  leds_write_all(0);
  LowPower.sleep();
  state = 0;
  time_start = millis();
  modes[mode].initialize(modes[mode].user_data);  
}

void on_wakeup() {
}

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
  LowPower.attachInterruptWakeup(BUTTON, on_wakeup, FALLING);
  time_start = millis();
}

void loop() {
  int runtime_ms = millis() - time_start;
  if (button_debounce() && runtime_ms > 50) {
      Serial.println("button pressed");
      led_clear();
      ++mode;
      time_start = millis();
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

  if (!digitalRead(POWER_USB) && (runtime_ms > RUN_DURATION_MS)) {
    on_sleep();
  }
}
