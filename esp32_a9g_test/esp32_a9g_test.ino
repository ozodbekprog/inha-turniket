// ============================================================
// A9G (GSM/GPRS) modem testi — ESP32-S3 bilan UART orqali
// Maqsad: modem aloqasi, SIM, tarmoq va internetni tekshirish
// ------------------------------------------------------------
// Ulanish:
//   A9G TX  -> ESP GPIO 6  (ESP RX)
//   A9G RX  -> ESP GPIO 7  (ESP TX)
//   A9G GND -> ESP GND (umumiy)
//   A9G VCC -> 3.7-4.2V ALohida manba (cho'qqida 2A!) ESP 3.3V dan EMAS
//   A9G PWR -> tugma 2s bosib yoqiladi (yoki GPIO 5 orqali)
//   GSM antennasi ulangan bo'lishi shart
// ------------------------------------------------------------
// APN operatorga qarab o'zgartiriladi (quyida APN_LIST).
// Yuklash (Arduino CLI):
//   --fqbn esp32:esp32:esp32s3:FlashSize=16M,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB
// ============================================================

#include <Arduino.h>

#define A9G_RX_PIN   6      // ESP RX  <- A9G TX
#define A9G_TX_PIN   7      // ESP TX  -> A9G RX
#define A9G_PWR_PIN  5      // ixtiyoriy: PWR kalitga ulangan bo'lsa
#define A9G_BAUD     115200

// Operator: "internet" ko'p SIMlarda ishlaydi; aniq APN lar ham qo'shildi
const char* APN_LIST[] = {"internet", "internet.ucell", "internet.beeline.uz"};
const int   APN_COUNT  = sizeof(APN_LIST) / sizeof(APN_LIST[0]);

const char* TEST_HOST = "api.ipify.org";   // javob: tashqi IP manzil
const int   TEST_PORT = 80;

HardwareSerial modem(1);
bool modemOk = false;

// ---------- YORDAMCHI: AT buyruq yuborish ----------
String modemCmd(const String& cmd, uint32_t timeoutMs, const char* expect = "OK") {
  while (modem.available()) modem.read();
  Serial.print(">> "); Serial.println(cmd);
  modem.print(cmd); modem.print("\r\n");
  String resp;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (modem.available()) resp += (char)modem.read();
    if (expect && resp.indexOf(expect) >= 0) break;
    delay(10);
  }
  resp.trim();
  if (resp.length()) { Serial.print("<< "); Serial.println(resp); }
  return resp;
}

// ---------- MODEM sinxronizatsiya ----------
// Ikkala TX/RX tartibi va turli baud tezliklarni sinab ko'radi
bool modemFind() {
  const uint32_t bauds[] = {115200, 9600, 57600, 38400};
  const int8_t pairs[2][2] = {
    {A9G_RX_PIN, A9G_TX_PIN},   // normal: RX=6, TX=7
    {A9G_TX_PIN, A9G_RX_PIN},   // teskari: RX=7, TX=6
  };
  uint32_t totalBytes = 0;

  for (int p = 0; p < 2; p++) {
    for (int b = 0; b < 4; b++) {
      modem.begin(bauds[b], SERIAL_8N1, pairs[p][0], pairs[p][1]);
      delay(150);
      while (modem.available()) modem.read();
      Serial.printf("[i] Sinal: RX=GPIO%d TX=GPIO%d baud=%u\n",
                    pairs[p][0], pairs[p][1], (unsigned)bauds[b]);
      for (int i = 0; i < 3; i++) {
        modem.print("AT\r\n");
        String resp;
        uint32_t t0 = millis();
        while (millis() - t0 < 700) {
          while (modem.available()) resp += (char)modem.read();
          delay(5);
        }
        totalBytes += resp.length();
        if (resp.length()) { Serial.print("<< "); Serial.println(resp); }
        if (resp.indexOf("OK") >= 0) {
          Serial.println("[i] MODEM TOPILDI!");
          modemCmd("ATE0", 1000);   // echo o'chirish
          return true;
        }
      }
    }
  }
  if (totalBytes == 0)
    Serial.println("[!] RX liniyada birorta bayt kelmadi -> A9G o'chiq yoki TX/RX butunlay boshqa pinlarda");
  else
    Serial.println("[!] Baytlar keldi, lekin javob OK emas -> baud/tartib mos emas");
  return false;
}

// ---------- SIM va tarmoq ----------
bool simNetworkCheck() {
  String cpin;
  for (int i = 0; i < 5; i++) {
    cpin = modemCmd("AT+CPIN?", 2500, "+CPIN");
    if (cpin.indexOf("READY") >= 0) break;
    delay(1500);
  }
  if (cpin.indexOf("READY") < 0) {
    if (cpin.indexOf("SIM PIN") >= 0) Serial.println("[!] SIM PIN bilan qulflangan");
    else Serial.println("[!] SIM topilmadi yoki qulflangan");
    return false;
  }
  Serial.println("[i] SIM: READY");

  for (int i = 0; i < 20; i++) {
    String csq = modemCmd("AT+CSQ", 2000, "+CSQ");
    Serial.print("[i] Signal (CSQ): "); Serial.println(csq);
    String reg = modemCmd("AT+CREG?", 2000, "+CREG");
    if (reg.indexOf(",1") >= 0 || reg.indexOf(",5") >= 0) {
      Serial.println("[i] Tarmoqqa ulandi");
      return true;
    }
    delay(2000);
  }
  Serial.println("[!] Tarmoqqa ulanmadi (antenna/SIM tekshiring)");
  return false;
}

// ---------- GPRS (internet) ----------
bool gprsConnect() {
  modemCmd("AT+CIPSHUT", 5000, "SHUT OK");
  for (int a = 0; a < APN_COUNT; a++) {
    Serial.printf("[i] APN sinovi: %s\n", APN_LIST[a]);
    String cmd = String("AT+CSTT=\"") + APN_LIST[a] + "\",\"\",\"\"";
    if (modemCmd(cmd, 3000).indexOf("OK") < 0) continue;
    modemCmd("AT+CIICR", 60000);
    String ip = modemCmd("AT+CIFSR", 5000, ".");
    ip.trim();
    if (ip.length() >= 7 && ip.indexOf("ERROR") < 0 && ip.indexOf("AT+") < 0) {
      Serial.print("[i] GPRS ulandi, ichki IP: "); Serial.println(ip);
      return true;
    }
  }
  Serial.println("[!] GPRS ulanmadi (APN yoki balans muammosi)");
  return false;
}

// ---------- TCP + HTTP GET testi ----------
bool tcpHttpGet(const char* host, int port, const String& path) {
  String start = String("AT+CIPSTART=\"TCP\",\"") + host + "\"," + port;
  while (modem.available()) modem.read();
  Serial.print(">> "); Serial.println(start);
  modem.print(start); modem.print("\r\n");

  String resp;
  uint32_t t0 = millis();
  bool connected = false;
  while (millis() - t0 < 60000) {
    while (modem.available()) resp += (char)modem.read();
    if (resp.indexOf("CONNECT OK") >= 0 || resp.indexOf("ALREADY CONNECT") >= 0) { connected = true; break; }
    if (resp.indexOf("CONNECT FAIL") >= 0 || resp.indexOf("ERROR") >= 0) break;
    delay(10);
  }
  Serial.print("<< "); Serial.println(resp);
  if (!connected) { Serial.println("[!] TCP ulanish amalga oshmadi"); return false; }
  Serial.println("[i] TCP ulandi");

  while (modem.available()) modem.read();
  Serial.println(">> AT+CIPSEND");
  modem.print("AT+CIPSEND\r\n");
  t0 = millis(); bool prompt = false;
  while (millis() - t0 < 5000) {
    while (modem.available()) if (modem.read() == '>') { prompt = true; break; }
    if (prompt) break;
    delay(10);
  }
  if (!prompt) { Serial.println("[!] CIPSEND prompt kelmadi"); return false; }

  String req = String("GET ") + path + " HTTP/1.1\r\nHost: " + host +
               "\r\nUser-Agent: inha-turniket\r\nConnection: close\r\n\r\n";
  modem.print(req);

  String r2;
  t0 = millis();
  bool sent = false;
  while (millis() - t0 < 15000) {
    while (modem.available()) {
      r2 += (char)modem.read();
      if (r2.indexOf("SEND OK") >= 0) sent = true;
    }
    if (sent && r2.indexOf("CLOSED") >= 0) break;
    delay(10);
  }
  Serial.print("<< "); Serial.println(r2);
  modemCmd("AT+CIPCLOSE", 5000);
  return sent;
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== A9G MODEM TESTI ===");
  pinMode(A9G_PWR_PIN, INPUT_PULLUP);   // PWR liniyani bo'sh qoldiramiz (tugma o'zi bosiladi)
  modem.begin(A9G_BAUD, SERIAL_8N1, A9G_RX_PIN, A9G_TX_PIN);
}

// ---------- LOOP ----------
void loop() {
  if (!modemOk) {
    Serial.println("[i] Modem qidirilmoqda...");
    modemOk = modemFind();
    if (modemOk) modemCmd("ATI", 2000, "OK");   // modem modeli
    else Serial.println("[!] Modem javob bermayapti: A9G PWR tugmasini 2-3 soniya bosib yoqing (tarmoq LED'i chaqnaguncha)");
    delay(3000);
    return;
  }
  if (!simNetworkCheck()) { modemOk = false; delay(5000); return; }
  if (!gprsConnect())    { modemOk = false; delay(5000); return; }

  Serial.println("[i] Internet testi: api.ipify.org");
  if (tcpHttpGet(TEST_HOST, TEST_PORT, "/")) {
    Serial.println("[i] INTERNET ISHLAYAPTI (javobdagi IP - tashqi IP)");
  }

  Serial.println("[i] 60s kutish...\n");
  delay(60000);
}
