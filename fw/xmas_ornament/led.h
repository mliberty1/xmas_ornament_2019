#include <stdint.h>
#include <stdbool.h>

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
#define POWER_USB A3

#define COMMANDS_PER_LED (128)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ADC_BITS (12)
#define LED_COUNT (10)
extern const uint8_t leds[LED_COUNT];

typedef void(*callback_fn)(void * user_data);

void led_initialize();
void led_clear();
void led_write(int led, int value);
void leds_write_all(int value);
bool led_is_idle(int led);
int led_random_from_idle();
void led_set(int led, int value);
void led_fade(int led, int value, int duration_ms);
void led_delay(int led, int duration_ms);
void led_delay_random(int led, int duration_min_ms, int duration_max_ms);
void led_signal(int led, callback_fn cbk_fn, void * cbk_user_data);
void led_animate(void * user_data);
