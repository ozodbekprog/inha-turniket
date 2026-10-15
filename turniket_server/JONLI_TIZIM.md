# INHA Turniket — Avtonom IoT tizimi (qo'llanma)

## Arxitektura (avtonom, har qanday holatda ishlaydi)

```
┌─────────────────────────────┐
│  ESP32-S3 (turniket)        │
│  • Lokal server: 192.168.4.1│  ← internet bo'lmasa ham ishlaydi!
│  • Navbat: FFat (yo'qolmaydi)│
│  • O'z-o'zini tiklash       │
└──────────┬──────────────────┘
           │ RX/TX (GPIO6/7)
     ┌─────▼─────┐
     │ A9G modem │──GPRS──► internet
     └───────────┘              │
                    Cloudflare tunnel (jonli URL)
                                │
                    ┌───────────▼───────────┐
                    │ Kompyuter (systemd):   │
                    │ • turniket-server      │
                    │ • turniket-tunnel      │
                    │ • turniket-bridge ─────┼──USB──► ESP (URL avto-yangilash)
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │ Jonli sayt + Mobil app │
                    └───────────────────────┘
```

## Har qanday holatda ishlash (failsafe)

| Holat | Nima bo'ladi |
|---|---|
| Internet yo'q | ESP lokal ishlaydi; hodisalar FFat navbatida saqlanadi |
| Modem qotdi | ESP PWR impuls bilan qayta yoqadi (o'z-o'zini tiklash) |
| Kompyuter o'chdi | ESP navbat yig'adi; kompyuter yonganda hammasi avto-tiklanadi |
| Tunnel URL o'zgardi | **turniket-bridge** yangi URL ni USB orqali ESP ga yuboradi (qayta yuklash shart emas!) |
| ESP qayta yuklandi | FFat'dan URL + navbat + o'quvchilar avto-yuklanadi |

## systemd xizmatlar (kompyuterda, avto-start)

```bash
systemctl --user status turniket-server   # lokal server (8080)
systemctl --user status turniket-tunnel   # cloudflared tunnel
systemctl --user status turniket-bridge   # URL avto-yangilagich
```

Boshqarish: `systemctl --user restart turniket-server` va h.k.

## ESP serial buyruqlari (USB orqali)

```bash
# Server manzilini yangilash (tunnel o'zgarganda bridge avtomatik yuboradi)
printf "SETHOST=xxxx.trycloudflare.com\n" > /dev/ttyACM0

# Holatni so'rash
printf "NET?\n" > /dev/ttyACM0
```

## API (ESP ichidagi server va jonli server — bir xil)

| Endpoint | Vazifa |
|---|---|
| `GET /api/data` | Dashboard (mobil app formati) |
| `GET /api/board` | Jonli sayt dashboardi |
| `GET /api/time?ms=…` | Vaqt sinxronizatsiyasi |
| `GET /api/del?uid=…` | O'quvchini o'chirish |
| `POST /add` | O'quvchi qo'shish (multipart, rasm) |
| `GET /api/export` | XLSX eksport |
| `GET /api/event?…` | ESP hodisa yuborishi |
| `GET /api/student?…` | ESP o'quvchi sinxronizatsiyasi |
| `GET /api/sethost?host=…` | ESP server manzilini o'zgartirish (WiFi orqali) |
| `GET /photos/<fayl>` | Rasm |

## Mobil app

- **To'liq app:** `flutter-app/build/app/outputs/flutter-apk/app-release.apk`
- **Yengil app:** `android-app/inha-turniket-admin.apk`
- WiFi: `INHA-TURNIKET` / `turniket2026` → `http://192.168.4.1`
- Masofadan: tunnel URL (ilova sozlamalarida kiritiladi)

## ESP firmware (muhim sozlamalar)

```cpp
#define NET_SERVER_HOST "…trycloudflare.com"   // standart (FFat /server.txt ustidan yozadi)
#define NET_UPLOAD_MS   60000                  // har 1 daqiqa
#define NET_SMS_NUMBER  ""                     // SMS ogohlantirish (ixtiyoriy)
```

APN: `a9g.h` da `A9G_APN = "internet.beeline.uz"`.

Yuklash:
```bash
arduino-cli compile --upload \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB" \
  -p /dev/ttyACM0 esp32_turniket
```

## Loglar

- `logs/cloudflared.log` — tunnel
- `logs/bridge.log` — URL yangilash
- `/tmp/turniket_server.log` — server (systemd journal ham: `journalctl --user -u turniket-server`)
