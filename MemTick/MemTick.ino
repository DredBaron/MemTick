/*
  MemTick: A 486 Heartbeat for the modern PC
  Firmware for MemTick Rev MC-A (ATtiny3216, SOIC-20)

  Recreates the AMI BIOS memory-count ticks and the POST beep each time
  the PC boots, driven by the PCIe PERST# signal.

  Build with megaTinyCore (Arduino IDE):
    Board:      ATtiny3226/3216/1626/1616/1606/826/816/806/426/416/406
    Chip:       ATtiny3216 (or ATtiny1616)
    Clock:      10 MHz internal  (safe at 3.3 V)
    millis():   enabled (default timer)
    BOD:        2.6 V or disabled
    Programmer: matches your UPDI programmer (microUPDI, SerialUPDI, ...)
  Never change the UPDI/reset pin fuse: keep "UPDI" selected.

  License: MIT (see LICENSE)

  ---------------------------------------------------------------
  Pin map (U1 pin : port : function)
    2  PA4  TURBO    (SW1)      active low
    3  PA5  LOUD     (SW1)      active low
    4  PA6  COLD     (SW1)      active low
    5  PA7  PROFILE  (SW1)      active low
    6  PB5  SW_B4  16 GB (SW2)  active low
    7  PB4  SW_B5  32 GB (SW2)  active low
    8  PB3  SW_B6  64 GB (SW2)  active low
    9  PB2  SW_B7 128 GB (SW2)  active low
   10  PB1  SW_B0   1 GB (SW3)  active low
   13  PC1  SW_B1   2 GB (SW3)  active low
   14  PC2  SW_B2   4 GB (SW3)  active low
   15  PC3  SW_B3   8 GB (SW3)  active low
   11  PB0  SPK_DRIVE -> R2 -> Q1 gate (high = speaker current on)
   12  PC0  PERST#  via R4 (low = PC held in reset)
   16  PA0  UPDI (programming only)
   19  PA3  activity LED via R6 (optional)
  ---------------------------------------------------------------
*/

#include <Arduino.h>

// ---------- pins ----------
#define PIN_TURBO    PIN_PA4
#define PIN_LOUD     PIN_PA5
#define PIN_COLD     PIN_PA6
#define PIN_PROFILE  PIN_PA7
#define PIN_SPK      PIN_PB0
#define PIN_PERST    PIN_PC0
#define PIN_LED      PIN_PA3

// Memory bits, index 0 = 1 GB ... index 7 = 128 GB
const uint8_t memPins[8] = {
  PIN_PB1, PIN_PC1, PIN_PC2, PIN_PC3,     // 1, 2, 4, 8 GB   (SW3)
  PIN_PB5, PIN_PB4, PIN_PB3, PIN_PB2      // 16, 32, 64, 128 GB (SW2)
};

// ---------- tuning (change to taste) ----------
const uint8_t  TICKS_PER_GB      = 16;    // one tick per "64 KB" scaled x1000
const uint16_t TICK_INTERVAL_MS  = 34;    // about 29 ticks per second (matches the analog design)
const uint16_t TURBO_INTERVAL_MS = 17;    // Turbo: about twice as fast
const uint16_t CLICK_LOUD_US     = 700;   // speaker pulse per tick, LOUD
const uint16_t CLICK_SOFT_US     = 180;   // speaker pulse per tick, SOFT
const uint16_t BEEP_HZ           = 896;   // IBM/AMI style POST beep
const uint16_t BEEP_MS           = 250;   // one short beep
const uint8_t  BEEP_DUTY_LOUD    = 50;    // percent of each cycle the coil is driven
const uint8_t  BEEP_DUTY_SOFT    = 15;
const uint16_t PRE_COUNT_DELAY_MS = 400;  // pause after PERST# releases, like a real POST
const uint16_t POST_COUNT_DELAY_MS = 250; // pause between last tick and the beep
const uint16_t PERST_DEBOUNCE_MS = 20;

// ---------- state ----------
static bool firstBootSincePower = true;

// ---------- helpers ----------
static inline void wdtKick() { __asm__ __volatile__("wdr"); }

static inline void speakerOff() { digitalWrite(PIN_SPK, LOW); }
static inline void speakerOn()  { digitalWrite(PIN_SPK, HIGH); }

static inline bool switchOn(uint8_t pin) { return digitalRead(pin) == LOW; }   // closed to GND = ON

static inline bool pcInReset() { return digitalRead(PIN_PERST) == LOW; }

static uint8_t readMemoryGB() {
  uint8_t gb = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (switchOn(memPins[i])) gb |= (1 << i);
  }
  return gb;
}

// Wait, but return false early if the PC goes back into reset.
static bool waitMs(uint16_t ms) {
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < ms) {
    wdtKick();
    if (pcInReset()) return false;
  }
  return true;
}

static void click(bool loud) {
  speakerOn();
  delayMicroseconds(loud ? CLICK_LOUD_US : CLICK_SOFT_US);
  speakerOff();
}

static void beep(bool loud) {
  const uint32_t periodUs = 1000000UL / BEEP_HZ;
  const uint8_t  duty     = loud ? BEEP_DUTY_LOUD : BEEP_DUTY_SOFT;
  const uint16_t onUs     = (uint16_t)(periodUs * duty / 100);
  const uint16_t offUs    = (uint16_t)(periodUs - onUs);
  const uint32_t cycles   = (uint32_t)BEEP_HZ * BEEP_MS / 1000;
  for (uint32_t i = 0; i < cycles; i++) {
    speakerOn();
    delayMicroseconds(onUs);
    speakerOff();
    delayMicroseconds(offUs);
    if ((i & 0x3F) == 0) wdtKick();
  }
  speakerOff();
}

// The memory count. Returns false if the PC reset part way through.
static bool memoryCount() {
  const uint8_t  gb      = readMemoryGB();
  const bool     turbo   = switchOn(PIN_TURBO);
  const bool     loud    = switchOn(PIN_LOUD);
  const bool     profile = switchOn(PIN_PROFILE);
  const uint16_t base    = turbo ? TURBO_INTERVAL_MS : TICK_INTERVAL_MS;
  const uint16_t total   = (uint16_t)gb * TICKS_PER_GB;   // up to 4080

  if (!waitMs(PRE_COUNT_DELAY_MS)) return false;

  for (uint16_t n = 0; n < total; n++) {
    // PROFILE on: start at half speed and ease up to full speed over the
    // first quarter of the count. PROFILE off: steady rate.
    uint16_t interval = base;
    if (profile) {
      const uint16_t ramp = total / 4;
      if (n < ramp) interval = base + (uint16_t)((uint32_t)base * (ramp - n) / ramp);
    }

    digitalWrite(PIN_LED, HIGH);
    click(loud);
    digitalWrite(PIN_LED, LOW);

    if (!waitMs(interval)) return false;
  }

  if (!waitMs(POST_COUNT_DELAY_MS)) return false;
  beep(loud);
  return true;
}

void setup() {
  // Speaker first: make sure the MOSFET is off before anything else.
  pinMode(PIN_SPK, OUTPUT);
  speakerOff();

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_TURBO, INPUT_PULLUP);
  pinMode(PIN_LOUD, INPUT_PULLUP);
  pinMode(PIN_COLD, INPUT_PULLUP);
  pinMode(PIN_PROFILE, INPUT_PULLUP);
  for (uint8_t i = 0; i < 8; i++) pinMode(memPins[i], INPUT_PULLUP);

  pinMode(PIN_PERST, INPUT);   // R3 pulls it up to 3.3 V on the card

  // Watchdog: about 2 s. If the firmware ever hangs, the chip resets and the
  // speaker pin returns to off.
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_2KCLK_gc);

  // Let the pull-ups settle before the switches are read.
  delay(5);
}

void loop() {
  // 1. Wait while the PC is held in reset.
  while (pcInReset()) wdtKick();

  // 2. PERST# just released: confirm it is stable.
  if (!waitMs(PERST_DEBOUNCE_MS)) return;

  // 3. COLD mode: only count on the first boot after power is applied.
  const bool coldOnly = switchOn(PIN_COLD);
  if (!coldOnly || firstBootSincePower) {
    memoryCount();
  }
  firstBootSincePower = false;

  // 4. Stay quiet until the next reset (warm reboot asserts PERST# again).
  while (!pcInReset()) wdtKick();
}
