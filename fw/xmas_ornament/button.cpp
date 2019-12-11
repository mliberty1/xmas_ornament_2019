
#include "button.h"
#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

struct button_state_s {
  bool debounce;
  uint64_t time_start_ms;
  int value_last;
};

button_state_s button_ = {false, 0, 1};

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
