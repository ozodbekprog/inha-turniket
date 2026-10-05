// ESP32-S3 + 3.5" ILI9488 TFT — TTPU logotipi fon + touch (iz qoldirmaydi)
// Ulanish: VCC=3V3, GND, CS=10, RESET=9, DC=8, MOSI=11, SCK=12, LED=3V3
// Touch: SDO/MISO=13, T_CLK=12, T_CS=14, T_DIN=11, T_DO=13
// Kutubxona: LovyanGFX

#include <LovyanGFX.hpp>
#include <Preferences.h>
#include "logo_image.h"

#define USE_TOUCH 1

#define PIN_SCK     12
#define PIN_MOSI    11
#define PIN_MISO    13
#define PIN_LCD_CS  10
#define PIN_LCD_DC   8
#define PIN_LCD_RST  9
#define PIN_T_CS    14

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel;
  lgfx::Bus_SPI       _bus;
#if USE_TOUCH
  lgfx::Touch_XPT2046 _touch;
#endif

public:
  LGFX(void) {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_SCK;
      cfg.pin_mosi    = PIN_MOSI;
      cfg.pin_miso    = USE_TOUCH ? PIN_MISO : -1;
      cfg.pin_dc      = PIN_LCD_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs           = PIN_LCD_CS;
      cfg.pin_rst          = PIN_LCD_RST;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.readable         = USE_TOUCH;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = USE_TOUCH;
      _panel.config(cfg);
    }
#if USE_TOUCH
    {
      auto cfg = _touch.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.freq        = 1000000;
      cfg.pin_sclk    = PIN_SCK;
      cfg.pin_mosi    = PIN_MOSI;
      cfg.pin_miso    = PIN_MISO;
      cfg.pin_cs      = PIN_T_CS;
      cfg.pin_int     = -1;
      cfg.bus_shared  = true;
      cfg.x_min       = 300;
      cfg.x_max       = 3900;
      cfg.y_min       = 400;
      cfg.y_max       = 3900;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
#endif
    setPanel(&_panel);
  }
};

LGFX lcd;
Preferences prefs;

uint32_t lastSec = UINT32_MAX;
bool touching = false;

void drawCornerLabels() {
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("ESP32-S3", 6, 6);
  lcd.drawString("3.5\" ILI9488", 6, 26);
}

void updateSeconds(bool force = false) {
  uint32_t sec = millis() / 1000;
  if (!force && sec == lastSec) return;
  lastSec = sec;

  char buf[24];
  snprintf(buf, sizeof(buf), "Sekund: %lu", (unsigned long)sec);
  lcd.fillRect(376, 6, 100, 22, 0x0293);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString(buf, 380, 6);
}

#if USE_TOUCH
void drawTouchIndicator(int32_t x, int32_t y) {
  char buf[40];
  snprintf(buf, sizeof(buf), "X:%4ld Y:%4ld", (long)x, (long)y);
  lcd.fillRect(6, 6, 150, 44, 0x0293);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_YELLOW);
  lcd.drawString("Tegildi", 6, 6);
  lcd.drawString(buf, 6, 26);

  lcd.drawCircle(x, y, 14, TFT_WHITE);
  lcd.drawCircle(x, y, 15, 0x0293);
  lcd.drawLine(x - 20, y, x + 20, y, TFT_WHITE);
  lcd.drawLine(x, y - 20, x, y + 20, TFT_WHITE);
  lcd.drawCircle(x, y, 2, TFT_RED);
}
#endif

void drawBackground(bool full = true) {
  lcd.drawJpg(logo_jpg, logo_jpg_len, 0, 0);
  drawCornerLabels();
  lastSec = UINT32_MAX;
  updateSeconds(true);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Logo fon yuklanmoqda...");

  lcd.init();
  lcd.setRotation(1);
  drawBackground();

#if USE_TOUCH
  uint16_t calData[8];
  prefs.begin("tftcal", true);
  size_t n = prefs.getBytes("cal", calData, sizeof(calData));
  prefs.end();
  if (n == sizeof(calData)) {
    lcd.setTouchCalibrate(calData);
    Serial.println("Touch kalibrovka NVS dan yuklandi");
  } else {
    Serial.println("Touch kalibrovka: ekrandagi nuqtalarni bosing");
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(TFT_WHITE);
    lcd.drawString("Kalibrovka: nuqtalarni qalam bilan bosing", 240, 40);
    lcd.calibrateTouch(calData, 0xFFFF, 0x0000, 20);
    prefs.begin("tftcal", false);
    prefs.putBytes("cal", calData, sizeof(calData));
    prefs.end();
    drawBackground();
  }
#endif

  Serial.println("Tayyor!");
}

void loop() {
  updateSeconds();

#if USE_TOUCH
  int32_t x, y;
  if (lcd.getTouch(&x, &y)) {
    touching = true;
    drawTouchIndicator(x, y);
    Serial.printf("touch x=%ld y=%ld\n", (long)x, (long)y);
  } else if (touching) {
    touching = false;
    drawBackground();   // fonni tozalab qayta chizamiz — iz qolmaydi
    Serial.println("qo'yib yuborildi, fon tiklandi");
  }
  delay(10);
#else
  delay(200);
#endif
}
