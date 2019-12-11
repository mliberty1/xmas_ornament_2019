// https://learn.adafruit.com/how-to-program-samd-bootloaders
// https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
// https://learn.adafruit.com/adafruit-feather-m0-basic-proto/using-with-arduino-ide
// https://github.com/bportaluri/ALA

#include "led.h"


const uint8_t leds[LED_COUNT] = {LED_10, LED_09, LED_08, LED_07, LED_06, LED_05, LED_02, LED_03, LED_04, LED_01};

struct button_state_s {
  bool debounce;
  uint64_t time_start_ms;
  int value_last;
};

button_state_s button_ = {false, 0, 1};
int mode = 0;
int state = 0;


void mode_sequential_analog() {
  for (int i = 0; i <= 255; ++i) {
    led_write(state, i);
    delayMicroseconds(1500);
  }
  delay(100);
  for (int i = 255; i >= 0; --i) {
    led_write(state, i);
    delayMicroseconds(1500);
  }
  ++state;
  if (state >= ARRAY_SIZE(leds)) {
    state = 0;
  }
}

void mode_set_all(void * user_data) {
  leds_write_all((int) user_data);
}

bool button_debounce() {
  int button_next = digitalRead(BUTTON);
  if (button_.debounce) {
    if ((millis() - button_.time_start_ms) >= 10) {
      button_.debounce = false;
      if (button_.value_last && !button_next) {
        button_.value_last = button_next;
        return true;
      } else {
        button_.value_last = button_next;
        return false;
      }
    }
  } else if (button_.value_last != button_next) {
    button_.time_start_ms = millis();
    button_.debounce = true;
  }
  return false;
}

void mode_animate_sequential(void * user_data) {
  int led = state;
  led_fade(led, 255, 300);
  led_delay(led, 300);
  led_fade(led, 0, 300);
  led_signal(led, mode_animate_sequential, 0);
  ++state;
  if (state >= LED_COUNT) {
    state = 0;
  }
}

void mode_animate_random(void * user_data) {
  int led;
  while (1) {
    led = random(LED_COUNT);
    if (led_is_idle(led)) {
      break;
    }
  }
  led_fade(led, 255, 300);
  led_signal(led, mode_animate_random, 0);
  led_delay(led, 300);
  led_fade(led, 0, 300);
}

void mode_wait_for_button(void * user_data) {
  // todo
}

struct mode_s {
  void * user_data;
  callback_fn initialize;
  callback_fn process;
};

struct mode_s modes[] = {
  {0, mode_animate_random, led_animate},
  {0, mode_animate_sequential, led_animate},
  {(void *) 0, mode_set_all, mode_wait_for_button}
};

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
}
