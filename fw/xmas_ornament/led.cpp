#include "led.h"
#include <string.h>  // memset
#include <math.h>
#include <Arduino.h>

int pwm_table[256];


struct led_state_s {
  bool active;
  unsigned long time_start_us;
  int value_last;
};

struct led_state_s led_state_[LED_COUNT];

enum led_command_e {
  LED_CMD_IDLE,
  LED_CMD_FADE,
  LED_CMD_DELAY,
  LED_CMD_SIGNAL,
};


struct led_command_s {
  enum led_command_e cmd;
  int value;
  int duration_us;
  callback_fn cbk_fn;
  void * cbk_user_data;
};

struct led_command_s led_commands_[LED_COUNT][COMMANDS_PER_LED];

static struct led_command_s * led_cmd_clear(struct led_command_s * c) {
  memset(c, 0, sizeof(*c));
  return c;
}

void led_initialize() {
  float r = 255 * log10f(2) / (log10f(powf(2, ADC_BITS) - 1));
  for (int i = 0; i < 256; ++i) {
    pwm_table[i] = (int) (round(powf(2, i / r) - 1));
  }
}

void led_clear() {
  for (int i = 0; i < LED_COUNT; ++i) {
    led_write(i, 0);
  }
  memset(&led_commands_, 0, sizeof(led_commands_));
  memset(&led_state_, 0, sizeof(led_state_));
}

void led_write(int led, int value) {
  if (value < 0) {
    value = 0;
  } else if (value > 255) {
    value = 255;
  }
  analogWrite(leds[led], pwm_table[value]);
}

void leds_write_all(int intensity) {
  for (int i = 0; i < LED_COUNT; ++i) {
    led_write(i, intensity);
  }
}

bool led_is_idle(int led) {
  return (led_commands_[led][0].cmd == LED_CMD_IDLE);
}

static struct led_command_s * led_cmd_next(int led) {
  if ((led < 0) || (led >= LED_COUNT)) {
    return 0;
  }
  for (int i = 0; i < COMMANDS_PER_LED; ++i) {
    if (led_commands_[led][i].cmd == LED_CMD_IDLE) {
      return led_cmd_clear(&led_commands_[led][i]);
    }
  }
  Serial.print("too many LED commands for ");
  Serial.print(led);
  Serial.println(" -- overwrite");
  return led_cmd_clear(&led_commands_[led][COMMANDS_PER_LED - 1]);
}

void led_set(int led, int value) {
  led_fade(led, value, 0);
}

void led_fade(int led, int value, int duration_ms) {
  struct led_command_s * c = led_cmd_next(led);
  if (c) {
    c->cmd = LED_CMD_FADE;
    c->value = value;
    c->duration_us = (unsigned long) (duration_ms * 1000);
  }
}

void led_delay(int led, int duration_ms) {
  struct led_command_s * c = led_cmd_next(led);
  if (c) {
    c->cmd = LED_CMD_DELAY;
    c->duration_us = (unsigned long) (duration_ms * 1000);
  }
}

void led_delay_random(int led, int duration_min_ms, int duration_max_ms) {
  int duration_ms = random(duration_max_ms - duration_min_ms) + duration_min_ms;
  led_delay(led, duration_ms);
}

void led_signal(int led, callback_fn cbk_fn, void * cbk_user_data) {
  if (!cbk_fn) {
    Serial.println("invalid callback");
    return;
  }
  struct led_command_s * c = led_cmd_next(led);
  if (c) {
    c->cmd = LED_CMD_SIGNAL;
    c->cbk_fn = cbk_fn;
    c->cbk_user_data = cbk_user_data;
  }
}

void cmd_finish(int led) {
  struct led_command_s * c = &led_commands_[led][0];
  if (c->cbk_fn) {
    c->cbk_fn(c->cbk_user_data);
  }
  if (c->cmd == LED_CMD_FADE) {
    led_write(led, c->value);
    led_state_[led].value_last = c->value;
  }
  for (int i = 1; i < COMMANDS_PER_LED; ++i) {
    led_commands_[led][i - 1] = led_commands_[led][i];
  }
  memset(&led_commands_[led][COMMANDS_PER_LED - 1], 0, sizeof(struct led_command_s));
  led_state_[led].active = false;
  led_state_[led].time_start_us = 0;
}

bool cmd_finish_on_duration(int led) {
  struct led_command_s * c = &led_commands_[led][0];
  unsigned int t = micros();
  if ((t - led_state_[led].time_start_us) >= c->duration_us) {
    cmd_finish(led);
    return true;
  }
  return false;
}

void led_animate(void * user_data) {
  for (int led = 0; led < LED_COUNT; ++led) {
    struct led_command_s * c = &led_commands_[led][0];
    if (!led_state_[led].active) { // start new command
      switch (c->cmd) {
        case LED_CMD_IDLE: continue;
        case LED_CMD_FADE: Serial.print("fade "); break;
        case LED_CMD_DELAY: Serial.print("delay "); break;
        case LED_CMD_SIGNAL: Serial.print("signal "); break;
      }
      led_state_[led].active = true;
      led_state_[led].time_start_us = micros();
      Serial.println(led_state_[led].time_start_us);
    } else {
      switch (c->cmd) {
        case LED_CMD_IDLE: break;
        case LED_CMD_FADE: 
          if (!cmd_finish_on_duration(led)) {
            float f = (micros() - led_state_[led].time_start_us) / (float) (c->duration_us);
            int v = (int) (f * (c->value - led_state_[led].value_last) + led_state_[led].value_last);
            Serial.print(c->value); Serial.print(" ");
            Serial.print(led_state_[led].value_last); Serial.print(" ");
            Serial.print(f); Serial.print(" ");
            Serial.print(v);Serial.println("");
            led_write(led, v);
          }
          break;
        case LED_CMD_DELAY: cmd_finish_on_duration(led); break; 
        case LED_CMD_SIGNAL: cmd_finish(led); break;
      }
    }
  }
}
