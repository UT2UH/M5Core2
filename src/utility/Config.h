#ifndef _CONFIG_H_
  #define _CONFIG_H_

  #define TFT     M5Display::instance
  #define BUTTONS	M5Buttons::instance

  // Screen
  #define TFT_LED_PIN 32
  #define TFT_DC_PIN 27
  #define TFT_CS_PIN 14
  #define TFT_MOSI_PIN 23
  #define TFT_CLK_PIN 18
  #define TFT_RST_PIN 33
  #define TFT_MISO_PIN 19

  // SD card
  #define TFCARD_CS_PIN 4

  // Modified M5Stack 868MHz LoRa RA-01H module
  // RA-1H LORA_RST_PIN has 4K7 pullup resistor
  // and 0.1uF capacitor to GND
  #define LORA_CS_PIN     26
  #define LORA_RST_PIN    -1
  #define LORA_IRQ_PIN    36

  // Modified M5Stack NEO-8N module
  // NEO-8N SDA connected to GPIO21(SYS_SDA)
  // NEO-8N SCL     - GPIO22(SYS_SCL)
  // NEO-8N EXTINT  - GPIO14
  #define GNSS_IRQ_PIN    35
  #define GNSS_EXTINT_PIN 14

  // PasPi DS3231SN#(?) breakout board installed
  // into M5Stack NEO-8N GPS module
  #define RTC_IRQ_PIN     13

  // UART
  #define USE_SERIAL Serial

  // Core2 defines
  #define M5Stack_M5Core2
  #define TFT_eSPI_TOUCH_EMULATION
  #define TOUCH		M5Touch::instance

#endif /* CONFIG_H */
