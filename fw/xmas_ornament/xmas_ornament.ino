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

#define MODES (2)


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ADC_BITS (12)

const byte leds[] = {LED_01, LED_04, LED_03, LED_02, LED_05, LED_06, LED_07, LED_08, LED_09, LED_10};
const int leds_len = ARRAY_SIZE(leds);

int pwm_table[256];

uint64_t button_time_start = 0;
int button_last = 0;
int mode = 0;
int state = 0;

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
  pinMode(BUTTON, INPUT);
  pinMode(POWER_USB, INPUT);
  if (digitalRead(POWER_USB)) {
    Serial.println("powered over USB");
  } else {
    Serial.println("powered by battery");
  }
}

void clear() {
  for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
    digitalWrite(leds[i], 0);
  }
  state = 0;
}

void mode_sequential_analog() {
  for (int i = 0; i <= 255; ++i) {
    analogWrite(leds[state], pwm_table[i]);
    delayMicroseconds(1500);
  }
  delay(100);
  for (int i = 255; i >= 0; --i) {
    analogWrite(leds[state], pwm_table[i]);
    delayMicroseconds(1500);
  }
  analogWrite(leds[state], 0);
  ++state;
  if (state >= ARRAY_SIZE(leds)) {
    state = 0;
  }
}

void mode_random() {
  int idx = random(leds_len);
  digitalWrite(leds[idx], 1);
  delay(10);
  digitalWrite(leds[idx], 0);
  delay(10);
  digitalWrite(leds[idx], 1);
  delay(50);
  digitalWrite(leds[idx], 0);
}

void mode_all_on(int intensity) {
  for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
    analogWrite(leds[i], intensity);
  }
}

void mode_dev() {
  ++state;
  
}

void mode_random_fade() {
  int idx = random(leds_len);
  analogWrite(leds[idx], 64);
  delay(50);
  analogWrite(leds[idx], 0);
}

void loop() {
  Serial.println("Loop");

  // button debounce
  int button_next = digitalRead(BUTTON);
  if (button_time_start == 0 && (button_last != digitalRead(BUTTON))) {
    button_time_start = millis();
  }
  if (button_time_start && (button_time_start + 20) > millis()) {
    button_time_start = 0;
    int button = digitalRead(BUTTON);
    if (button != button_last) {
      button_last = button;
      Serial.print("button ");
      Serial.println(button);
      clear();
      ++mode;
      if (mode >= MODES) {
        mode = 0;
      }
    }
  }

  switch (mode) {
    case 0: mode_sequential_analog(); break;
    case 1: mode_random(); break;
    default: mode_sequential_analog(); break;
    
  }
}
