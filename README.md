# INHA University — Aqlli Turniket (Smart Access Turnstile)

ESP32-S3 asosidagi RFID turniket: talaba karta urilganda ekranda uning **rasmi, ismi, guruhi** va **KIRDI/CHIQDI** animatsiyasi chiqadi. Qurilma **o'zi WiFi tarqatadi** va **dashboard ESP ichida** ishlaydi — internet ham, kompyuter ham kerak emas (mustaqil/standalone).

## Xususiyatlar

- 📶 **INHA-TURNIKET** WiFi tarmog'i (parol: `turniket2026`), dashboard: `http://192.168.4.1` yoki `http://turniket.local`
- 🪪 RC522 RFID (13.56 MHz) — karta 1 sekunddan tez tekshiriladi
- 🖼 3.5" ILI9488 TFT (480x320): logo fon, rasm bilan tasdiqlash, KIRDI/CHIQDI holati
- ⚡ Rasm kesh — takroriy urishda bir zumda ko'rsatiladi
- 💾 Talabalar, rasmlar va davomat ESP xotirasida (FFat) saqlanadi
- 🌐 Dashboard: o'quvchi qo'shish (rasm yuklash), o'chirish, davomat jadvali, jonli statistika
- 🚫 Bloklangan kartalar rad etiladi; notanish karta UID'si ekranda ko'rsatiladi (ro'yxatga olish uchun)
- ☁️ Bonus: Google Sheets (Apps Script) varianti — `esp32_turniket/apps_script.js`

## Uskunalar

| Qism | Izoh |
|---|---|
| ESP32-S3 DevKitC-1 | 16MB flash, 8MB PSRAM |
| ILI9488 3.5" SPI TFT | 320x480, rezistiv touch (XPT2046) |
| RC522 RFID | 3.3V, 13.56 MHz |

## Ulanish

### Display (SPI3/HSPI)

| TFT moduli | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | GPIO10 |
| RESET | GPIO9 |
| DC/RS | GPIO8 |
| SDI/MOSI | GPIO11 |
| SCK | GPIO12 |
| LED | 3V3 |
| SDO/MISO | GPIO13 (touch uchun) |

### RC522 (SPI2/FSPI, alohida shina)

| RC522 | ESP32-S3 |
|---|---|
| 3.3V | 3V3 |
| GND | GND |
| SDA/SS | GPIO15 |
| SCK | GPIO16 |
| MOSI | GPIO17 |
| MISO | GPIO18 |
| RST | GPIO4 |
| IRQ | — (bo'sh) |

## Qurish va yuklash

Kutubxonalar:
```
arduino-cli lib install LovyanGFX
arduino-cli lib install MFRC522
```

FQBN:
```
esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc
```

Buyruqlar:
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc" esp32_turniket
arduino-cli upload  -p /dev/ttyACM0 --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc" esp32_turniket
```

## Ishlatish

1. Qurilma yoqiladi → `INHA-TURNIKET` WiFi'ga ulanadi (parol `turniket2026`)
2. Brauzerda `http://192.168.4.1` ochiladi
3. **Yangi o'quvchi qo'shish**: UID + Ism + Familiya + Guruh + rasm yuklanadi
   - Notanish kartani urib, ekranda chiqqan UID'ni shu yerga kiritish mumkin
4. Karta uriladi → TFT'da rasm bilan tasdiq, dashboardda davomat yangilanadi

## Loyiha tuzilishi

```
esp32_turniket/            # Asosiy firmware (mustaqil rejim)
  esp32_turniket.ino       #   WiFi AP + WebServer + dashboard + RC522 + TFT
  logo_image.h             #   Inha logotipi foni (JPEG)
  bek_photo.h              #   Demo o'quvchi rasmi
  apps_script.js           #   Google Sheets varianti (Apps Script)
  clasp/                   #   clasp orqali deploy uchun
esp32_logo/                # Logo fon demo loyihasi
turniket_server/           # Kompyuterda ishlaydigan server varianti (Python)
```

## Muallif

Inha University in Tashkent — turniket loyihasi (2026).
