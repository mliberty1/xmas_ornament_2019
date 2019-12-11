// https://learn.adafruit.com/how-to-program-samd-bootloaders
// https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
// https://learn.adafruit.com/adafruit-feather-m0-basic-proto/using-with-arduino-ide
// https://github.com/bportaluri/ALA

#define LED_01 12
#define LED_02 13
#define LED_03 10
#define LED_04 11
#define LED_05 5
#define LED_06 2
#define LED_07 0
#define LED_08 1
#define LED_09 3
#define LED_10 4
#define BUTTON A1
#define POWER_USB A3

#define COMMANDS_PER_LED (128)


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ADC_BITS (12)

const byte leds[] = {LED_10, LED_09, LED_08, LED_07, LED_06, LED_05, LED_02, LED_03, LED_04, LED_01};
#define LED_COUNT (ARRAY_SIZE(leds))

int pwm_table[256];

struct button_state_s {
  bool debounce;
  uint64_t time_start_ms;
  int value_last;
};

button_state_s button_ = {false, 0, 1};
int mode = 0;
int state = 0;

struct led_state_s {
  bool active;
  unsigned long time_start_us;
  int value_last;
};

struct led_state_s led_state_[ARRAY_SIZE(leds)];

enum led_command_e {
  LED_CMD_IDLE,
  LED_CMD_FADE,
  LED_CMD_DELAY,
  LED_CMD_SIGNAL,
};

typedef void(*callback_fn)(void * user_data);

struct led_command_s {
  enum led_command_e cmd;
  int value;
  int duration_us;
  callback_fn cbk_fn;
  void * cbk_user_data;
};

struct led_command_s led_commands_[ARRAY_SIZE(leds)][COMMANDS_PER_LED];

struct led_command_s * led_cmd_clear(struct led_command_s * c) {
  memset(c, 0, sizeof(*c));
  return c;
}

bool led_is_idle(int led) {
  return (led_commands_[led][0].cmd == LED_CMD_IDLE);
}

struct led_command_s * led_cmd_next(int led_idx) {
  if ((led_idx < 0) || (led_idx >= ARRAY_SIZE(leds))) {
    return 0;
  }
  for (int i = 0; i < COMMANDS_PER_LED; ++i) {
    if (led_commands_[led_idx][i].cmd == LED_CMD_IDLE) {
      return led_cmd_clear(&led_commands_[led_idx][i]);
    }
  }
  Serial.print("too many LED commands for ");
  Serial.print(led_idx);
  Serial.println(" -- overwrite");
  return led_cmd_clear(&led_commands_[led_idx][COMMANDS_PER_LED - 1]);
}

void led_fade(int led_idx, int value, int duration_ms) {
  struct led_command_s * c = led_cmd_next(led_idx);
  if (c) {
    c->cmd = LED_CMD_FADE;
    c->value = value;
    c->duration_us = (unsigned long) (duration_ms * 1000);
  }
}

void led_delay(int led_idx, int duration_ms) {
  struct led_command_s * c = led_cmd_next(led_idx);
  if (c) {
    c->cmd = LED_CMD_DELAY;
    c->duration_us = (unsigned long) (duration_ms * 1000);
  }
}

void led_signal(int led_idx, callback_fn cbk_fn, void * cbk_user_data) {
  if (!cbk_fn) {
    Serial.println("invalid callback");
    return;
  }
  struct led_command_s * c = led_cmd_next(led_idx);
  if (c) {
    c->cmd = LED_CMD_SIGNAL;
    c->cbk_fn = cbk_fn;
    c->cbk_user_data = cbk_user_data;
  }
}

void led_write(int led_idx, int value) {
  if (value < 0) {
    Serial.println("value underflow");
    value = 0;
  } else if (value > 255) {
    Serial.println("value overflow");
    value = 255;
  }
  analogWrite(leds[led_idx], pwm_table[value]);
}

void clear() {
  for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
    led_write(i, 0);
  }
  memset(&led_commands_, 0, sizeof(led_commands_));
  memset(&led_state_, 0, sizeof(led_state_));
  state = 0;
}

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

void mode_random() {
  int idx = random(LED_COUNT);
  led_write(idx, 255);
  delay(10);
  led_write(idx, 0);
  delay(10);
  led_write(idx, 255);
  delay(50);
  led_write(idx, 0);
}

void leds_all_on(int intensity) {
  for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
    led_write(i, intensity);
  }
}

void mode_all_on(void * user_data) {
  leds_all_on((int) user_data);
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
    Serial.print("cmd_finish_on_duration ");
    Serial.print(t);
    Serial.print(" ");
    Serial.print(led_state_[led].time_start_us);
    Serial.print(" ");
    Serial.print(c->duration_us);
    Serial.println("");
    cmd_finish(led);
    return true;
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

void animate(void * user_data) {
  for (int led = 0; led < ARRAY_SIZE(leds); ++led) {
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

struct mode_s {
  void * user_data;
  callback_fn initialize;
  callback_fn process;
};

struct mode_s modes[] = {
  {0, mode_animate_random, animate},
  {0, mode_animate_sequential, animate},
  //{0, NULL, mode_sequential_analog},
  //{0, NULL, mode_random},
  {(void *) 64, mode_all_on, NULL}
};

void setup() {
  analogWriteResolution(ADC_BITS);
  float r = 255 * log10(2) / (log10(pow(2, ADC_BITS) - 1));
  for (int i = 0; i < 256; ++i) {
    pwm_table[i] = (int) (round(pow(2, i / r) - 1));
  }
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
      clear();
      ++mode;
      if (mode >= ARRAY_SIZE(modes)) {
        mode = 0;
      }
      if (modes[mode].initialize) {
        modes[mode].initialize(modes[mode].user_data);
      }
  }

  if (modes[mode].process) {
    modes[mode].process(modes[mode].user_data);
  }
}
