#include "led.h"
#include <string.h>  // memset
#include <math.h>
#include <Arduino.h>
#include "wiring_private.h"

int pwm_table[256];


struct tc_isr_update_s {
  int8_t active;
  uint16_t cc;
};

struct tc_isr_update_s tc_update[1][2];

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

void TC3_Handler (void) {
  Tc* TCx = TC3;
  TC3->COUNT16.INTFLAG.bit.MC0 = 1;
  TC3->COUNT16.INTFLAG.bit.MC1 = 1;
  for (uint8_t channel = 0; channel < 2; channel++) {
    if (tc_update[0][channel].active) {
      tc_update[0][channel].active = 0;
      TCx->COUNT16.CC[channel].reg = tc_update[0][channel].cc;
    }
  }
}

void led_initialize() {
  float r = 255 * log10f(2) / (log10f(powf(2, ADC_BITS) - 1));
  for (int i = 0; i < 256; ++i) {
    pwm_table[i] = (int) (round(powf(2, i / r) - 1));
  }
  for (int i = 0; i < LED_COUNT; ++i) {
    analogWrite(leds[i], 0);
  }
  led_clear();

  NVIC_DisableIRQ(TC3_IRQn);
  NVIC_ClearPendingIRQ(TC3_IRQn);
  NVIC_SetPriority(TC3_IRQn, 0);
  NVIC_EnableIRQ(TC3_IRQn);
  TC3->COUNT16.INTENSET.bit.MC0 = 1;
  TC3->COUNT16.INTENSET.bit.MC1 = 1;
}

void led_clear() {
  memset(&tc_update, 0, sizeof(tc_update));
  memset(&led_commands_, 0, sizeof(led_commands_));
  memset(&led_state_, 0, sizeof(led_state_));
  leds_write_all(0);
}

// Wait for synchronization of registers between the clock domains
static __inline__ void syncTC_16(Tc* TCx) __attribute__((always_inline, unused));
static void syncTC_16(Tc* TCx) {
  while (TCx->COUNT16.STATUS.bit.SYNCBUSY);
}

// Wait for synchronization of registers between the clock domains
static __inline__ void syncTCC(Tcc* TCCx) __attribute__((always_inline, unused));
static void syncTCC(Tcc* TCCx) {
  while (TCCx->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);
}

void pwm_write(int pin, int value) {
  PinDescription pinDesc = g_APinDescription[pin];
  uint32_t attr = pinDesc.ulPinAttribute;

  if ((attr & PIN_ATTR_PWM) == PIN_ATTR_PWM) {
    uint32_t tcNum = GetTCNumber(pinDesc.ulPWMChannel);
    uint8_t tcChannel = GetTCChannelNumber(pinDesc.ulPWMChannel);
    static bool tcEnabled[TCC_INST_NUM+TC_INST_NUM];
    if (attr & PIN_ATTR_TIMER) {
      pinPeripheral(pin, PIO_TIMER);
    } else if ((attr & PIN_ATTR_TIMER_ALT) == PIN_ATTR_TIMER_ALT){
      //this is on an alt timer
      pinPeripheral(pin, PIO_TIMER_ALT);
    } else {
      return;
    }

    if (tcNum >= TCC_INST_NUM) {
      Tc* TCx = (Tc*) GetTC(pinDesc.ulPWMChannel);
      noInterrupts();
      uint16_t value_prev = TCx->COUNT16.CC[tcChannel].reg;
      uint16_t count = TCx->COUNT16.COUNT.reg;
      if ((value < value_prev) && (count < value_prev)) {
        interrupts();
        tc_update[0][tcChannel].cc = value;
        tc_update[0][tcChannel].active = 1;
      } else {
        TCx->COUNT16.CC[tcChannel].reg = (uint32_t) value;
        interrupts();
        syncTC_16(TCx);
      }
    } else {
      Tcc* TCCx = (Tcc*) GetTC(pinDesc.ulPWMChannel);
      TCCx->CTRLBSET.bit.LUPD = 1;
      syncTCC(TCCx);
      TCCx->CCB[tcChannel].reg = (uint32_t) value;
      syncTCC(TCCx);
      TCCx->CTRLBCLR.bit.LUPD = 1;
      syncTCC(TCCx);
    }
  }
}

void led_write(int led, int value) {
  int pin = leds[led];
  if (value < 0) {
    value = 0;
  } else if (value > 255) {
    value = 255;
  }
  value = pwm_table[value];
  pwm_write(pin, value);
}

void leds_write_all(int intensity) {
  for (int i = 0; i < LED_COUNT; ++i) {
    led_write(i, intensity);
  }
}

bool led_is_idle(int led) {
  return (led_commands_[led][0].cmd == LED_CMD_IDLE);
}

int led_random_from_idle() {
  int led = -1;
  while (1) {
    led = random(LED_COUNT);
    if (led_is_idle(led)) {
      return led;
    }
  }
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

void cmd_start_next(int led) {
  struct led_command_s * c = &led_commands_[led][0];
  switch (c->cmd) {
    case LED_CMD_IDLE: return;
    case LED_CMD_FADE: Serial.print("fade "); break;
    case LED_CMD_DELAY: Serial.print("delay "); break;
    case LED_CMD_SIGNAL: Serial.print("signal "); break;
  }
  led_state_[led].active = true;
  led_state_[led].time_start_us = micros();
  // Serial.println(led_state_[led].time_start_us);
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
  cmd_start_next(led);
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
      cmd_start_next(led);
    } else {
      switch (c->cmd) {
        case LED_CMD_IDLE: break;
        case LED_CMD_FADE: 
          if (!cmd_finish_on_duration(led)) {
            float f = (micros() - led_state_[led].time_start_us) / (float) (c->duration_us);
            int v = (int) (f * (c->value - led_state_[led].value_last) + led_state_[led].value_last);
            /*
            Serial.print(c->value); Serial.print(" ");
            Serial.print(led_state_[led].value_last); Serial.print(" ");
            Serial.print(f); Serial.print(" ");
            Serial.print(v);Serial.println("");
            */
            led_write(led, v);
          } // else c is no longer valid!
          break;
        case LED_CMD_DELAY: cmd_finish_on_duration(led); break; 
        case LED_CMD_SIGNAL: cmd_finish(led); break;
      }
    }
  }
}
