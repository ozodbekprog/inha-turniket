// A9G GSM/GPRS IoT moduli — ESP32-S3 uchun
// UART: RX=GPIO6, TX=GPIO7, 115200
// Vazifasi: modemni topish, tarmoqqa/GPRS ga ulanish, HTTP GET yuborish
#pragma once
#include <Arduino.h>

#define A9G_RX_PIN   6
#define A9G_TX_PIN   7
#define A9G_BAUD     115200
#define A9G_PWR_PIN  5

void a9gPowerPulse() {
  Serial.println("[A9G] PWR impuls (power-cycle)...");
  pinMode(A9G_PWR_PIN, OUTPUT);
  digitalWrite(A9G_PWR_PIN, LOW);
  delay(2200);
  pinMode(A9G_PWR_PIN, INPUT_PULLUP);
  delay(1000);
}

static const char* A9G_APN = "internet.beeline.uz";   // Beeline UZ (tasdiqlangan)

HardwareSerial a9gSerial(1);
bool a9gModemFound = false;
bool a9gGprsUp = false;

String a9gCmd(const String& cmd, uint32_t timeoutMs, const char* expect = "OK") {
  while (a9gSerial.available()) a9gSerial.read();
  a9gSerial.print(cmd); a9gSerial.print("\r\n");
  String resp;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (a9gSerial.available()) resp += (char)a9gSerial.read();
    if (expect && (resp.indexOf(expect) >= 0 || resp.indexOf("ERROR") >= 0 ||
                   resp.indexOf("CONNECT FAIL") >= 0)) break;
    delay(10);
  }
  resp.trim();
  return resp;
}

bool a9gFind() {
  a9gSerial.begin(A9G_BAUD, SERIAL_8N1, A9G_RX_PIN, A9G_TX_PIN);
  delay(200);
  for (int i = 0; i < 5; i++) {
    String r = a9gCmd("AT", 1200, "OK");
    if (r.indexOf("OK") >= 0) {
      a9gCmd("ATE0", 1000, "OK");
      Serial.println("[A9G] Modem topildi");
      return true;
    }
    delay(500);
  }
  Serial.println("[A9G] Modem javob bermayapti");
  return false;
}

bool a9gEnsureOnline() {
  if (!a9gModemFound) {
    a9gModemFound = a9gFind();
    if (!a9gModemFound) return false;
  }
  a9gCmd("ATE0", 1000, "OK");

  if (a9gGprsUp) {
    String ipr = a9gCmd("AT+CGPADDR=1", 3000, "+CGPADDR");
    if (ipr.indexOf("+CGPADDR") >= 0 && ipr.indexOf("0.0.0.0") < 0) return true;
    a9gGprsUp = false;
  }

  String cpin = a9gCmd("AT+CPIN?", 2500, "+CPIN");
  if (cpin.indexOf("READY") < 0) {
    Serial.println("[A9G] SIM topilmadi (NO SIM CARD?)");
    a9gModemFound = false;
    return false;
  }

  bool reg = false;
  for (int i = 0; i < 15; i++) {
    String r = a9gCmd("AT+CREG?", 2000, "+CREG");
    if (r.indexOf(",1") >= 0 || r.indexOf(",5") >= 0) { reg = true; break; }
    delay(2000);
  }
  if (!reg) { Serial.println("[A9G] Tarmoqqa ulanmadi"); return false; }
  Serial.println("[A9G] Tarmoq: ulandi");

  // APN kontekst — CGDCONT faqat DEAKTIV holatda o'zgartiriladi
  String cgd = a9gCmd("AT+CGDCONT?", 3000, "+CGDCONT");
  if (cgd.indexOf(A9G_APN) < 0) {
    Serial.println("[A9G] APN sozlanmoqda...");
    a9gCmd("AT+CGACT=0,1", 8000);
    String set = a9gCmd(String("AT+CGDCONT=1,\"IP\",\"") + A9G_APN + "\"", 5000);
    Serial.println("[A9G] CGDCONT: " + set);
  }

  a9gCmd("AT+CGATT=1", 10000, "+CGATT");
  a9gCmd("AT+CGACT=1,1", 15000);
  String ipr = a9gCmd("AT+CGPADDR=1", 5000, "+CGPADDR");
  Serial.println("[A9G] CGPADDR: " + ipr);
  a9gGprsUp = ipr.indexOf("+CGPADDR") >= 0 && ipr.indexOf("0.0.0.0") < 0;
  Serial.println(a9gGprsUp ? "[A9G] GPRS tayyor" : "[A9G] GPRS xato");
  return a9gGprsUp;
}

bool a9gHttpGet(const char* host, uint16_t port, const String& path, String& resp,
                uint32_t timeoutMs = 25000) {
  resp = "";
  a9gCmd("AT+CIPCLOSE", 2000, "OK");
  a9gCmd("AT+CIPMUX=0", 2000, "OK");

  static String cachedIP = "";
  bool connected = false;
  const uint16_t ports[] = {8080, 80, 8880};   // Cloudflare HTTP portlari (8080 birinchi)
  for (int pi = 0; pi < 3 && !connected; pi++) {
    if (cachedIP.length() == 0) {
      String dns = a9gCmd(String("AT+CDNSGIP=\"") + host + "\"", 15000, "+CDNSGIP");
      int q1 = dns.indexOf("\",\"");
      if (dns.indexOf("+CDNSGIP: 1") >= 0 && q1 >= 0) {
        int q2 = dns.indexOf("\"", q1 + 3);
        if (q2 > q1) {
          cachedIP = dns.substring(q1 + 3, q2);
          Serial.println("[A9G] DNS -> " + cachedIP);
        }
      }
    }
    String target = cachedIP.length() ? cachedIP : String(host);
    String start = String("AT+CIPSTART=\"TCP\",\"") + target + "\"," + ports[pi];
    String r = a9gCmd(start, 15000, "CONNECT OK");
    Serial.printf("[A9G] CIPSTART(%s:%u): %s\n", target.c_str(), ports[pi], r.c_str());
    if (r.indexOf("CONNECT OK") >= 0 || r.indexOf("ALREADY CONNECT") >= 0) {
      connected = true;
    } else {
      delay(1000);
    }
  }
  if (!connected) {
    cachedIP = "";
    Serial.println("[A9G] TCP ulanish amalga oshmadi");
    return false;
  }

  String req = String("GET ") + path + " HTTP/1.1\r\nHost: " + host +
               "\r\nUser-Agent: inha-turniket\r\nConnection: close\r\n\r\n";

  while (a9gSerial.available()) a9gSerial.read();
  a9gSerial.print(String("AT+CIPSEND=") + req.length() + "\r\n");
  bool prompt = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    while (a9gSerial.available()) {
      if (a9gSerial.read() == '>') { prompt = true; break; }
    }
    if (prompt) break;
    delay(10);
  }
  if (!prompt) { Serial.println("[A9G] CIPSEND prompt yo'q"); return false; }

  a9gSerial.print(req);

  t0 = millis();
  bool done = false;
  while (millis() - t0 < timeoutMs) {
    while (a9gSerial.available()) resp += (char)a9gSerial.read();
    if (resp.indexOf("CLOSED") >= 0) { done = true; break; }
    delay(10);
  }
  // +CIPRCV,NNN: markerlarini tozalash (matn buzilmasligi uchun)
  {
    int ci;
    while ((ci = resp.indexOf("+CIPRCV,")) >= 0) {
      int j = ci + 8;
      while (j < (int)resp.length() && resp[j] >= '0' && resp[j] <= '9') j++;
      if (j < (int)resp.length() && resp[j] == ':') resp.remove(ci, j - ci + 1);
      else resp.remove(ci, 8);
    }
  }
  Serial.printf("[A9G] HTTP (%u): %.140s\n", (unsigned)resp.length(), resp.c_str());
  a9gCmd("AT+CIPCLOSE", 2000, "OK");
  return done || resp.indexOf("200") >= 0;
}
