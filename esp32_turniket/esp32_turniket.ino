// INHA UNIVERSITY — MUSTAQIL (standalone) ESP32-S3
// - O'zi "INHA-TURNIKET" WiFi tarqatadi
// - Dashboard ESP ichida: http://192.168.4.1  (yoki http://turniket.local)
// - Talabalar/rasmlar/davomat ESP xotirasida (FFat)
// - RC522 karta o'qish + ILI9488 3.5" ekran + logo fon
// Kutubxonalar: LovyanGFX, MFRC522

#include <LovyanGFX.hpp>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <FFat.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <map>
#include "logo_image.h"
#include "bek_photo.h"

// ---------- SOZLAMALAR ----------
#define AP_SSID  "INHA-TURNIKET"
#define AP_PASS  "turniket2026"

// ---------- PINLAR ----------
#define PIN_SCK     12
#define PIN_MOSI    11
#define PIN_MISO    13
#define PIN_LCD_CS  10
#define PIN_LCD_DC   8
#define PIN_LCD_RST  9
#define PIN_T_CS    14

#define RFID_CS     15
#define RFID_SCK    16
#define RFID_MOSI   17
#define RFID_MISO   18
#define RFID_RST     4

#define CARD_HOLD_MS 7000

// ---------- DISPLAY ----------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Touch_XPT2046 _touch;
public:
  LGFX(void) {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI3_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_SCK;
      cfg.pin_mosi    = PIN_MOSI;
      cfg.pin_miso    = PIN_MISO;
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
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      _panel.config(cfg);
    }
    {
      auto cfg = _touch.config();
      cfg.spi_host    = SPI3_HOST;
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
    setPanel(&_panel);
  }
};

LGFX lcd;
WebServer server(80);
MFRC522 rfid(RFID_CS, RFID_RST);

bool rfidOk = false, rfidSwapBuses = false;
bool cardShowing = false;
uint32_t cardShownAt = 0;
uint32_t lastClock = UINT32_MAX;
String lastUid = "";
uint32_t lastUidAt = 0;

// Uyqu (sleep) rejimi
Preferences prefs;
bool sleeping = false;
bool touching = false;
uint32_t lastActivity = 0;
#define SLEEP_MS 60000UL

uint8_t* photoBuf = nullptr;
size_t   photoLen = 0;
String   cachedUid = "";

bool clockSet = false;
File uploadFile;
bool uploadStarted = false;

struct Student {
  String uid, ism, familiya, sid, guruh, holat, photo;
};
std::vector<Student> students;

// ---- Ro'yxatdan olingan o'quvchilar (karta UID -> ism) ----
struct SeedStudent {
  const char* uid;
  const char* ism;
  const char* familiya;
  const char* sid;
};
static const SeedStudent SEED_LIST[] = {
  {"14DFE66A", "Amina",       "Ixromova",       "U2610266"},
  {"47F0FA6A", "Mohinurbonu", "Murodova",       "U2610263"},
  {"F787EA6A", "Kira",        "Janbayeva",      "U2610275"},
  {"A778F46A", "Mo'mina",     "Xudoyberdiyeva", "U2610274"},
  {"0A75FD8A", "Davlat",      "Boltayev",       "U2610229"},
  {"F7BEF86A", "Odina",       "Voxidova",       "U2610237"},
  {"F7D5E5CA", "Saliha",      "Shavkatova",     "U2610249"},
  {"F7A3EECA", "Edem",        "Saidumarov",     "U2610221"},
  {"0B48DF8A", "Toxir",       "Baradirov",      "U2610213"},
  {"4D661F6C", "Ali",         "Yuldashev",      "U2610239"},
  {"E710F86A", "Saidamir",    "Abidov",         "U2610272"},
  {"B76CE36A", "Vyacheslav",  "Kim",            "U2610260"},
  {"874FFB6A", "Yakov",       "An",             "U2610271"},
  {"471DF76A", "Nadira",      "Xakimova",       "U2610240"},
  {"6701FE6A", "Sayyora",     "Reymova",        "U2610262"},
  {"2775E5A6", "Sanjarbek",   "Yeshbayev",      "U2610268"},
  {"075EF86A", "Muhammadamin","Oripov",         "U2610243"},
  {"82ABF86A", "Zohir",       "Shermatov",      "U2610265"},
  {"57D8FA6A", "Amrulla",     "Barakayev",      "U2610252"},
};
static const int SEED_LIST_COUNT = sizeof(SEED_LIST) / sizeof(SEED_LIST[0]);

struct LogEntry {
  String ts, uid, ism, guruh, holat;
};
std::vector<LogEntry> logs;                 // RAM (oxirgi ~300)
std::map<String, String> lastState;         // uid -> Kirdi/Chiqdi

#define BG_BLUE 0x0293
#define GRN     0x07E0
#define RED     0xF800
#define YEL     0xFFE0
#define PANEL   0x10A2

// =================== YORDAMCHI ===================

String nowString() {
  if (clockSet) {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return String(buf);
  }
  char buf[24];
  snprintf(buf, sizeof(buf), "T+%lus", (unsigned long)(millis() / 1000));
  return String(buf);
}

String clockShort() {
  if (clockSet) {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[12];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return String(buf);
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "Sek:%lu", (unsigned long)(millis() / 1000));
  return String(buf);
}

String trimField(const String& s) {
  String r = s;
  r.replace(",", " ");
  r.replace("\n", " ");
  r.replace("\r", " ");
  r.trim();
  return r;
}

String jsonEsc(const String& s) {
  String r = "";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { r += '\\'; r += c; }
    else if (c == '\n') r += "\\n";
    else if ((uint8_t)c >= 32) r += c;
  }
  return r;
}

// =================== FAYLLAR ===================

void saveStudents() {
  File f = FFat.open("/students.csv", "w");
  if (!f) return;
  for (auto& s : students) {
    f.printf("%s,%s,%s,%s,%s,%s,%s\n", s.uid.c_str(), s.ism.c_str(),
             s.familiya.c_str(), s.sid.c_str(), s.guruh.c_str(),
             s.holat.c_str(), s.photo.c_str());
  }
  f.close();
}

void loadStudents() {
  students.clear();
  File f = FFat.open("/students.csv", "r");
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 5) continue;
    String parts[7];
    int idx = 0, start = 0;
    for (int i = 0; i <= (int)line.length() && idx < 7; i++) {
      if (i == (int)line.length() || line[i] == ',') {
        parts[idx++] = line.substring(start, i);
        start = i + 1;
      }
    }
    Student s;
    if (idx >= 7) {
      s.uid = parts[0]; s.ism = parts[1]; s.familiya = parts[2];
      s.sid = parts[3]; s.guruh = parts[4]; s.holat = parts[5]; s.photo = parts[6];
    } else {
      s.uid = parts[0]; s.ism = parts[1]; s.familiya = parts[2];
      s.sid = ""; s.guruh = parts[3]; s.holat = parts[4]; s.photo = parts[5];
    }
    s.uid.trim(); s.ism.trim(); s.familiya.trim(); s.sid.trim();
    s.guruh.trim(); s.holat.trim(); s.photo.trim();
    if (s.uid.length() == 0) continue;
    if (s.holat.length() == 0) s.holat = "faol";
    students.push_back(s);
    Serial.printf("  #%d %s | %s %s | %s\n", (int)students.size(),
                  s.uid.c_str(), s.ism.c_str(), s.familiya.c_str(), s.sid.c_str());
  }
  f.close();
  Serial.printf("O'quvchilar yuklandi: %d\n", (int)students.size());
}

// Boshlang'ich ro'yxatni qo'shish (bir marta)
void seedStudents() {
  if (FFat.exists("/seed_v2.done")) return;
  int added = 0;
  for (int i = 0; i < SEED_LIST_COUNT; i++) {
    bool exists = false;
    for (auto& st : students) {
      if (st.uid.equalsIgnoreCase(SEED_LIST[i].uid)) { exists = true; break; }
    }
    if (!exists) {
      students.push_back(Student{String(SEED_LIST[i].uid), String(SEED_LIST[i].ism),
                                 String(SEED_LIST[i].familiya), String(SEED_LIST[i].sid),
                                 "", "faol", ""});
      added++;
    }
  }
  saveStudents();
  File f = FFat.open("/seed_v2.done", "w");
  if (f) { f.println("ok"); f.close(); }
  Serial.printf("Seed: %d ta yangi o'quvchi qo'shildi\n", added);
}

void appendLogFile(const String& ts, const String& uid, const String& ism,
                   const String& guruh, const String& holat) {
  File f = FFat.open("/logs.csv", "a");
  if (f) {
    f.printf("%s,%s,%s,%s,%s\n", ts.c_str(), uid.c_str(), ism.c_str(), guruh.c_str(), holat.c_str());
    f.close();
  }
  // Kirdi/Chiqdi alohida fayllarga ham yoziladi
  if (holat == "Kirdi" || holat == "Chiqdi") {
    File g = FFat.open(holat == "Kirdi" ? "/kirdi.csv" : "/chiqdi.csv", "a");
    if (g) {
      g.printf("%s,%s,%s,%s\n", ts.c_str(), uid.c_str(), ism.c_str(), guruh.c_str());
      g.close();
    }
  }
  LogEntry e{ts, uid, ism, guruh, holat};
  logs.push_back(e);
  if (logs.size() > 300) logs.erase(logs.begin());
}

// Bitta o'quvchiga bir nechta karta (UID'lar ';' bilan ajratiladi)
bool uidMatches(const String& stored, const String& scanned) {
  int start = 0;
  while (start <= (int)stored.length()) {
    int semi = stored.indexOf(';', start);
    String part = (semi < 0) ? stored.substring(start) : stored.substring(start, semi);
    part.trim();
    if (part.equalsIgnoreCase(scanned)) return true;
    if (semi < 0) break;
    start = semi + 1;
  }
  return false;
}

// Qo'shimcha kartani biriktirish (bir marta)
void patchCards() {
  if (FFat.exists("/patch1.done")) return;
  for (auto& s : students) {
    if (s.uid.indexOf("00B9B41A") >= 0 && s.uid.indexOf("97709F25") < 0) {
      s.uid += ";97709F25";
      saveStudents();
      Serial.println("Patch: 97709F25 Bekmirzaga biriktirildi");
      break;
    }
  }
  File f = FFat.open("/patch1.done", "w");
  if (f) { f.println("ok"); f.close(); }
}

// Eski davomat ma'lumotlarini bir marta tozalash
void wipeOldData() {
  if (FFat.exists("/clear_1.done")) return;
  FFat.remove("/logs.csv");
  FFat.remove("/kirdi.csv");
  FFat.remove("/chiqdi.csv");
  logs.clear();
  lastState.clear();
  File f = FFat.open("/clear_1.done", "w");
  if (f) { f.println("ok"); f.close(); }
  Serial.println("Eski davomat ma'lumotlari o'chirildi");
}

void loadLogs() {
  logs.clear();
  lastState.clear();
  File f = FFat.open("/logs.csv", "r");
  if (!f) return;
  int total = 0;
  std::vector<String> all;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 5) continue;
    total++;
    if (all.size() < 4000) all.push_back(line);
  }
  f.close();
  // Juda ko'p bo'lsa qisqartiramiz
  if (total > 2000) {
    File w = FFat.open("/logs.csv", "w");
    if (w) {
      size_t startI = all.size() > 1000 ? all.size() - 1000 : 0;
      for (size_t i = startI; i < all.size(); i++) w.println(all[i]);
      w.close();
    }
  }
  for (auto& line : all) {
    String parts[5];
    int idx = 0, start = 0;
    for (int i = 0; i <= (int)line.length() && idx < 5; i++) {
      if (i == (int)line.length() || line[i] == ',') parts[idx++] = line.substring(start, i), start = i + 1;
    }
    LogEntry e{parts[0], parts[1], parts[2], parts[3], parts[4]};
    if (e.holat == "Kirdi") lastState[e.uid] = "Kirdi";
    else if (e.holat == "Chiqdi") lastState[e.uid] = "Chiqdi";
  }
  size_t startI = all.size() > 300 ? all.size() - 300 : 0;
  for (size_t i = startI; i < all.size(); i++) {
    String line = all[i];
    String parts[5];
    int idx = 0, start = 0;
    for (int j = 0; j <= (int)line.length() && idx < 5; j++) {
      if (j == (int)line.length() || line[j] == ',') parts[idx++] = line.substring(start, j), start = j + 1;
    }
    LogEntry e{parts[0], parts[1], parts[2], parts[3], parts[4]};
    logs.push_back(e);
  }
  Serial.printf("Loglar: %d (RAM: %d)\n", total, (int)logs.size());
}

int insideCount() {
  int c = 0;
  for (auto& kv : lastState) if (kv.second == "Kirdi") c++;
  return c;
}

int deniedCount() {
  int c = 0;
  for (auto& e : logs) if (e.holat == "Ruxsat yo'q" || e.holat == "Bloklangan") c++;
  return c;
}

// =================== WEB: DASHBOARD ===================

#include "dash_html.h"

String studentState(const String& storedUid, String& viaUid) {
  String st = ""; viaUid = "";
  int start = 0;
  while (start <= (int)storedUid.length()) {
    int semi = storedUid.indexOf(';', start);
    String part = (semi < 0) ? storedUid.substring(start) : storedUid.substring(start, semi);
    part.trim();
    auto it = lastState.find(part);
    if (it != lastState.end()) { st = it->second; viaUid = part; }
    if (semi < 0) break;
    start = semi + 1;
  }
  return st;
}

String apiData() {
  String today = "";
  if (clockSet) {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    char b[12];
    strftime(b, sizeof(b), "%Y-%m-%d", &tm);
    today = String(b);
  }

  int todayK = 0, todayC = 0;
  int hk[24] = {0}, hc[24] = {0};
  for (auto& e : logs) {
    bool isToday = (!clockSet) || e.ts.startsWith(today);
    if (e.holat == "Kirdi") { if (isToday) todayK++; }
    else if (e.holat == "Chiqdi") { if (isToday) todayC++; }
    if (clockSet && isToday && e.ts.length() >= 13) {
      int h = e.ts.substring(11, 13).toInt();
      if (h >= 0 && h < 24) {
        if (e.holat == "Kirdi") hk[h]++;
        else if (e.holat == "Chiqdi") hc[h]++;
      }
    }
  }

  String j = "{";
  j += "\"total\":" + String((int)students.size());
  j += ",\"inside\":" + String(insideCount());
  j += ",\"todayKirdi\":" + String(todayK);
  j += ",\"todayChiqdi\":" + String(todayC);
  j += ",\"hoursK\":[";
  for (int i = 0; i < 24; i++) { if (i) j += ","; j += String(hk[i]); }
  j += "],\"hoursC\":[";
  for (int i = 0; i < 24; i++) { if (i) j += ","; j += String(hc[i]); }
  j += "]";

  // Hozir ichida
  j += ",\"insideList\":[";
  bool first = true;
  for (auto& kv : lastState) {
    if (kv.second != "Kirdi") continue;
    for (auto& s : students) {
      if (!uidMatches(s.uid, kv.first)) continue;
      String ts = "";
      for (int i = (int)logs.size() - 1; i >= 0; i--) {
        if (logs[i].uid == kv.first && logs[i].holat == "Kirdi") { ts = logs[i].ts; break; }
      }
      if (!first) j += ",";
      first = false;
      j += "{\"ism\":\"" + jsonEsc(s.ism) + "\",\"familiya\":\"" + jsonEsc(s.familiya) +
           "\",\"sid\":\"" + jsonEsc(s.sid) + "\",\"photo\":\"" + jsonEsc(s.photo) +
           "\",\"ts\":\"" + jsonEsc(ts) + "\"}";
      break;
    }
  }
  j += "]";

  // Kirdi (ichida) va Chiqdi statusidagi o'quvchilar
  j += ",\"kirdiList\":[";
  { bool f2 = true;
    for (auto& s : students) {
      String via; String st = studentState(s.uid, via);
      if (st != "Kirdi") continue;
      String ts = "";
      for (int i = (int)logs.size() - 1; i >= 0; i--)
        if (logs[i].uid == via && logs[i].holat == "Kirdi") { ts = logs[i].ts; break; }
      if (!f2) j += ","; f2 = false;
      j += "{\"ts\":\"" + jsonEsc(ts) + "\",\"ism\":\"" + jsonEsc(s.ism) +
           "\",\"familiya\":\"" + jsonEsc(s.familiya) + "\",\"sid\":\"" + jsonEsc(s.sid) +
           "\",\"photo\":\"" + jsonEsc(s.photo) + "\",\"holat\":\"Kirdi\"}";
    }
  }
  j += "],\"chiqdiList\":[";
  { bool f2 = true;
    for (auto& s : students) {
      String via; String st = studentState(s.uid, via);
      if (st != "Chiqdi") continue;
      String ts = "";
      for (int i = (int)logs.size() - 1; i >= 0; i--)
        if (logs[i].uid == via && logs[i].holat == "Chiqdi") { ts = logs[i].ts; break; }
      if (!f2) j += ","; f2 = false;
      j += "{\"ts\":\"" + jsonEsc(ts) + "\",\"ism\":\"" + jsonEsc(s.ism) +
           "\",\"familiya\":\"" + jsonEsc(s.familiya) + "\",\"sid\":\"" + jsonEsc(s.sid) +
           "\",\"photo\":\"" + jsonEsc(s.photo) + "\",\"holat\":\"Chiqdi\"}";
    }
  }
  j += "]";

  // Barcha o'quvchilar
  j += ",\"students\":[";
  for (size_t i = 0; i < students.size(); i++) {
    if (i) j += ",";
    j += "{\"uid\":\"" + jsonEsc(students[i].uid) + "\",";
    j += "\"ism\":\"" + jsonEsc(students[i].ism) + "\",";
    j += "\"familiya\":\"" + jsonEsc(students[i].familiya) + "\",";
    j += "\"sid\":\"" + jsonEsc(students[i].sid) + "\",";
    j += "\"photo\":\"" + jsonEsc(students[i].photo) + "\"}";
  }
  j += "]";
  j += "}";
  return j;
}

void handleData() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", apiData());
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handleTime() {
  if (server.hasArg("ms")) {
    time_t secs = (time_t)(server.arg("ms").toDouble() / 1000.0);
    struct timeval tv = {secs, 0};
    settimeofday(&tv, nullptr);
    clockSet = true;
    lastClock = UINT32_MAX;
  }
  server.send(200, "application/json", "{\"ok\":1}");
}

void handleUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadStarted = true;
    uploadFile = FFat.open("/photos/_tmp.jpg", "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  }
}

void handleAddDone() {
  String uid = trimField(server.arg("uid"));
  uid.toUpperCase();
  String ism = trimField(server.arg("ism"));
  String familiya = trimField(server.arg("familiya"));
  String sid = trimField(server.arg("sid"));
  String guruh = trimField(server.arg("guruh"));

  String photo = "";
  for (auto& s : students) if (s.uid == uid) photo = s.photo;

  if (uploadStarted && uid.length()) {
    String clean = uid;
    clean.replace(" ", "");
    photo = clean + ".jpg";
    FFat.remove("/photos/" + photo);
    FFat.rename("/photos/_tmp.jpg", "/photos/" + photo);
  }
  uploadStarted = false;

  bool found = false;
  for (auto& s : students) {
    if (s.uid == uid) {
      s.ism = ism; s.familiya = familiya; s.guruh = guruh;
      if (sid.length()) s.sid = sid;
      if (photo.length()) s.photo = photo;
      found = true;
      break;
    }
  }
  if (!found && uid.length() && ism.length()) {
    students.push_back(Student{uid, ism, familiya, sid, guruh, "faol", photo});
  }
  saveStudents();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "OK");
}

void handleDelete() {
  String uid = trimField(server.arg("uid"));
  for (size_t i = 0; i < students.size(); i++) {
    if (students[i].uid == uid) {
      if (students[i].photo.length()) FFat.remove("/photos/" + students[i].photo);
      students.erase(students.begin() + i);
      break;
    }
  }
  saveStudents();
  server.send(200, "application/json", "{\"ok\":1}");
}

void handleReset() {
  FFat.remove("/logs.csv");
  FFat.remove("/kirdi.csv");
  FFat.remove("/chiqdi.csv");
  logs.clear();
  lastState.clear();
  server.send(200, "application/json", "{\"ok\":1}");
}

// =================== EXCEL (.xlsx) EKSPORT ===================
// ESP'da minimal ZIP (store) + XLSX generator

uint32_t crc32Of(const String& s) {
  uint32_t crc = 0xFFFFFFFF;
  const uint8_t* p = (const uint8_t*)s.c_str();
  for (size_t i = 0; i < s.length(); i++) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
  }
  return crc ^ 0xFFFFFFFF;
}
void put16(String& s, uint16_t v) { s += (char)(v & 0xFF); s += (char)(v >> 8); }
void put32(String& s, uint32_t v) {
  s += (char)(v & 0xFF); s += (char)((v >> 8) & 0xFF);
  s += (char)((v >> 16) & 0xFF); s += (char)((v >> 24) & 0xFF);
}
String xmlEsc(const String& s) {
  String r = "";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') r += "&amp;";
    else if (c == '<') r += "&lt;";
    else if (c == '>') r += "&gt;";
    else if (c == '"') r += "&quot;";
    else if (c == '\'') r += "&apos;";
    else if ((uint8_t)c >= 32) r += c;
  }
  return r;
}
String xmlRow(const char* const* cells, int n) {
  String r = "<row>";
  for (int i = 0; i < n; i++) {
    r += "<c t=\"inlineStr\"><is><t xml:space=\"preserve\">";
    r += xmlEsc(String(cells[i]));
    r += "</t></is></c>";
  }
  r += "</row>\r\n";
  return r;
}

String buildSheetDavomat() {
  String sh;
  sh.reserve(110000);
  sh += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  sh += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
  const char* hdr[5] = {"Vaqt", "UID", "Ism", "Guruh", "Holat"};
  sh += xmlRow(hdr, 5);
  File f = FFat.open("/logs.csv", "r");
  if (f) {
    int total = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n'); line.trim();
      if (line.length() > 4) total++;
    }
    f.seek(0);
    int skip = total > 1000 ? total - 1000 : 0;
    int seen = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n'); line.trim();
      if (line.length() <= 4) continue;
      seen++;
      if (seen <= skip) continue;
      String parts[5], tmp;
      int idx = 0, start = 0;
      for (int i = 0; i <= (int)line.length() && idx < 5; i++) {
        if (i == (int)line.length() || line[i] == ',') {
          parts[idx++] = line.substring(start, i);
          start = i + 1;
        }
      }
      for (int i = idx; i < 5; i++) parts[i] = "";
      const char* cells[5] = {parts[0].c_str(), parts[1].c_str(), parts[2].c_str(),
                              parts[3].c_str(), parts[4].c_str()};
      sh += xmlRow(cells, 5);
    }
    f.close();
  }
  sh += "</sheetData></worksheet>";
  return sh;
}

String buildSheetStudents() {
  String sh;
  sh.reserve(12000);
  sh += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  sh += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
  const char* hdr[6] = {"UID", "Ism", "Familiya", "Talaba ID", "Guruh", "Holat"};
  sh += xmlRow(hdr, 6);
  for (auto& s : students) {
    const char* cells[6] = {s.uid.c_str(), s.ism.c_str(), s.familiya.c_str(),
                            s.sid.c_str(), s.guruh.c_str(), s.holat.c_str()};
    sh += xmlRow(cells, 6);
  }
  sh += "</sheetData></worksheet>";
  return sh;
}

void handleExport() {
  String ct = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
    "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
    "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
    "<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
    "</Types>";
  String rels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
    "</Relationships>";
  String wb = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
    "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>"
    "<sheet name=\"Davomat\" sheetId=\"1\" r:id=\"rId1\"/>"
    "<sheet name=\"Oquvchilar\" sheetId=\"2\" r:id=\"rId2\"/>"
    "</sheets></workbook>";
  String wbrels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
    "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
    "</Relationships>";

  String s1 = buildSheetDavomat();
  String s2 = buildSheetStudents();

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Content-Disposition", "attachment; filename=\"inha-turniket.xlsx\"");
  server.send(200, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "");

  struct E { String name; uint32_t crc, size, off; };
  std::vector<E> dir;
  uint32_t offset = 0;

  auto addPart = [&](const char* name, String& data) {
    uint32_t crc = crc32Of(data);
    String lh;
    put32(lh, 0x04034b50); put16(lh, 20); put16(lh, 0); put16(lh, 0);
    put16(lh, 0); put16(lh, 0x21);
    put32(lh, crc); put32(lh, data.length()); put32(lh, data.length());
    put16(lh, strlen(name)); put16(lh, 0);
    lh += name;
    server.sendContent(lh);
    server.sendContent(data);
    E e; e.name = name; e.crc = crc; e.size = data.length(); e.off = offset;
    dir.push_back(e);
    offset += lh.length() + data.length();
    data = String();
  };

  addPart("[Content_Types].xml", ct);
  addPart("_rels/.rels", rels);
  addPart("xl/workbook.xml", wb);
  addPart("xl/_rels/workbook.xml.rels", wbrels);
  addPart("xl/worksheets/sheet1.xml", s1);
  addPart("xl/worksheets/sheet2.xml", s2);

  String cd;
  cd.reserve(600);
  for (auto& e : dir) {
    put32(cd, 0x02014b50); put16(cd, 20); put16(cd, 20); put16(cd, 0); put16(cd, 0);
    put16(cd, 0); put16(cd, 0x21);
    put32(cd, e.crc); put32(cd, e.size); put32(cd, e.size);
    put16(cd, e.name.length()); put16(cd, 0); put16(cd, 0);
    put16(cd, 0); put16(cd, 0); put32(cd, 0); put32(cd, e.off);
    cd += e.name;
  }
  server.sendContent(cd);
  String eocd;
  put32(eocd, 0x06054b50); put16(eocd, 0); put16(eocd, 0);
  put16(eocd, dir.size()); put16(eocd, dir.size());
  put32(eocd, cd.length()); put32(eocd, offset); put16(eocd, 0);
  server.sendContent(eocd);
  server.sendContent("");
}

void handleNotFound() {
  String path = server.uri();
  if (path.startsWith("/photos/")) {
    String fname = path.substring(8);
    if (fname.indexOf("..") >= 0 || fname.length() == 0) { server.send(404); return; }
    File f = FFat.open("/photos/" + fname, "r");
    if (!f) { server.send(404, "text/plain", "topilmadi"); return; }
    server.streamFile(f, "image/jpeg");
    f.close();
    return;
  }
  server.send(404, "text/plain", "topilmadi");
}

// =================== EKRAN ===================

void drawCornerLabels() {
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("INHA UNIVERSITY", 6, 6);

  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(GRN);
  lcd.drawString("WiFi: INHA-TURNIKET", 6, 30);
  lcd.setTextColor(TFT_CYAN);
  lcd.drawString("192.168.4.1", 6, 44);

  lcd.setTextColor(rfidOk ? GRN : RED);
  lcd.drawString(rfidOk ? "RFID: tayyor" : "RFID: topilmadi", 6, 58);
}

void drawHint() {
  lcd.setTextDatum(textdatum_t::bottom_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("Kartani taqdim eting", 160, 476);
}

void updateClock(bool force = false) {
  uint32_t now = millis() / 1000;
  if (!force && now == lastClock) return;
  lastClock = now;
  lcd.fillRect(226, 4, 90, 20, BG_BLUE);
  lcd.setTextDatum(textdatum_t::top_right);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString(clockShort(), 314, 4);

  // Vaqtni davriy ravishda xotiraga saqlash (qayta yoqilganda tiklash uchun)
  static uint32_t lastEpochSave = 0;
  if (clockSet && millis() - lastEpochSave > 60000) {
    lastEpochSave = millis();
    prefs.begin("rtc", false);
    prefs.putUInt("epoch", (uint32_t)time(nullptr));
    prefs.end();
  }
}

void drawBackground() {
  lcd.drawJpg(logo_jpg, logo_jpg_len, 0, 0);
  drawCornerLabels();
  drawHint();
  lcd.fillRect(226, 4, 90, 20, BG_BLUE);
  lastClock = UINT32_MAX;
  updateClock(true);
}

// =================== UYQU (SLEEP) ===================

void updateSleepClock(bool force = false) {
  static uint32_t lastSleepSec = UINT32_MAX;
  uint32_t sec = millis() / 1000;
  if (!force && sec == lastSleepSec) return;
  lastSleepSec = sec;

  lcd.fillRect(20, 175, 280, 75, TFT_BLACK);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextSize(2);
  lcd.setTextColor(0x52AA, TFT_BLACK);   // xira soat
  lcd.drawString(clockShort(), 160, 212);
  lcd.setTextSize(1);
}

void drawSleepScreen() {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(0x2124, TFT_BLACK);   // juda xira
  lcd.drawString("INHA UNIVERSITY", 160, 120);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(0x2124, TFT_BLACK);
  lcd.drawString("Turniket kutish rejimida", 160, 155);
  updateSleepClock(true);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(0x18E3, TFT_BLACK);
  lcd.drawString("Kartani taqdim eting", 160, 460);
}

void goSleep() {
  sleeping = true;
  drawSleepScreen();
  Serial.println("Sleep rejimi (ekranda xira soat)");
}

void wakeUp() {
  if (!sleeping) return;
  sleeping = false;
  drawBackground();
  Serial.println("Uyg'ondi");
}

void drawTouchIndicator(int32_t x, int32_t y) {
  char buf[40];
  snprintf(buf, sizeof(buf), "X:%4ld Y:%4ld", (long)x, (long)y);
  lcd.fillRect(6, 6, 150, 44, BG_BLUE);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(YEL);
  lcd.drawString("Tegildi", 6, 6);
  lcd.drawString(buf, 6, 26);
  lcd.drawCircle(x, y, 12, TFT_WHITE);
  lcd.drawLine(x - 16, y, x + 16, y, TFT_WHITE);
  lcd.drawLine(x, y - 16, x, y + 16, TFT_WHITE);
  lcd.drawCircle(x, y, 2, TFT_RED);
}

bool loadPhotoFromFS(const String& file) {
  photoLen = 0;
  if (photoBuf) { free(photoBuf); photoBuf = nullptr; }
  File f = FFat.open("/photos/" + file, "r");
  if (!f) return false;
  size_t cap = 150000;
  photoBuf = (uint8_t*)ps_malloc(cap);
  if (!photoBuf) photoBuf = (uint8_t*)malloc(cap);
  if (!photoBuf) { f.close(); return false; }
  photoLen = f.read(photoBuf, cap);
  f.close();
  return photoLen > 100;
}

void showResultPanel(const String& uid, const String& name, const String& sid,
                     const String& group, bool known, bool allowed, const char* action) {
  cardShowing = true;
  cardShownAt = millis();

  uint16_t color = allowed ? GRN : RED;

  lcd.fillRoundRect(8, 36, 304, 408, 14, PANEL);
  lcd.drawRoundRect(8, 36, 304, 408, 14, color);
  lcd.drawRoundRect(11, 39, 298, 402, 12, color);

  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(color, PANEL);
  lcd.drawString(allowed ? "XUSH KELIBSIZ!" : "RUXSAT YO'Q", 160, 62);

  // Rasm (150x150, markazda)
  int px = 85, py = 86, ps = 150;
  lcd.drawRect(px, py, ps, ps, 0x8410);
  if (photoLen > 100) {
    lcd.drawJpg(photoBuf, photoLen, px + 1, py + 1, ps - 2, ps - 2);
  } else {
    lcd.fillRect(px + 1, py + 1, ps - 2, ps - 2, 0x18E3);
    lcd.setFont(&fonts::Font0);
    lcd.setTextColor(0xC618, 0x18E3);
    lcd.drawString(known ? "RASM YO'Q" : "NOTANISH", 160, py + ps / 2);
  }

  char buf[64];
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(TFT_WHITE, PANEL);
  if (known) {
    snprintf(buf, sizeof(buf), "%s", name.c_str());
    if (strlen(buf) > 17) { buf[15] = '.'; buf[16] = '.'; buf[17] = '.'; buf[18] = 0; }
  } else {
    snprintf(buf, sizeof(buf), "Karta bazada yo'q");
  }
  lcd.drawString(buf, 160, 264);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(YEL, PANEL);
  snprintf(buf, sizeof(buf), "Talaba ID: %s", sid.length() ? sid.c_str() : "-");
  lcd.drawString(buf, 160, 300);

  lcd.setTextColor(TFT_CYAN, PANEL);
  snprintf(buf, sizeof(buf), "Guruh: %s", group.length() ? group.c_str() : "-");
  lcd.drawString(buf, 160, 324);

  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(0xC618, PANEL);
  snprintf(buf, sizeof(buf), "Karta: %s", uid.c_str());
  lcd.drawString(buf, 160, 346);

  char tbuf[32] = "";
  snprintf(tbuf, sizeof(tbuf), "%s", nowString().c_str());
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(0xC618, PANEL);
  lcd.drawString(tbuf, 160, 366);

  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(color, PANEL);
  lcd.drawString(action, 160, 402);
}

// =================== RFID ===================

bool rfidVersionOk(byte v) {
  if (v == 0x00 || v == 0xFF) return false;
  return (v == 0x91 || v == 0x92 || v == 0x82 || v == 0x88 || v == 0x12);
}

byte rfidReadVersion() {
  delay(10);
  byte v1 = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  delay(5);
  byte v2 = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  return (v1 == v2) ? v1 : 0x00;
}

void rfidHardwareInit() {
  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
  rfid.PCD_Init();
  delay(50);
  byte ver = rfidReadVersion();
  Serial.printf("RFID probe: 0x%02X\n", ver);
  if (rfidVersionOk(ver)) { rfidSwapBuses = false; rfidOk = true; return; }
  SPI.end();
  delay(10);
  SPI.begin(RFID_SCK, RFID_MOSI, RFID_MISO, RFID_CS);
  rfid.PCD_Init();
  delay(50);
  ver = rfidReadVersion();
  Serial.printf("RFID probe (swap): 0x%02X\n", ver);
  if (rfidVersionOk(ver)) { rfidSwapBuses = true; rfidOk = true; return; }
  SPI.end();
  delay(10);
  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
  rfid.PCD_Init();
  rfidSwapBuses = false;
  rfidOk = false;
}

String uidToString(MFRC522::Uid& u) {
  char buf[3];
  String s = "";
  for (byte i = 0; i < u.size; i++) {
    snprintf(buf, sizeof(buf), "%02X", u.uidByte[i]);
    s += buf;
  }
  return s;
}

void handleCard() {
  String uid = uidToString(rfid.uid);
  Serial.printf("Karta: %s\n", uid.c_str());
  rfid.PICC_HaltA();

  sleeping = false;   // karta urilganda uyg'onadi
  lastActivity = millis();

  int idx = -1;
  for (size_t i = 0; i < students.size(); i++) {
    if (uidMatches(students[i].uid, uid)) { idx = (int)i; break; }
  }
  bool known = idx >= 0;
  bool allowed = false;
  String name = "", group = "", photoFile = "", sid = "";
  if (known) {
    allowed = !(students[idx].holat.equalsIgnoreCase("bloklangan") ||
                students[idx].holat.equalsIgnoreCase("blok"));
    name  = students[idx].ism + " " + students[idx].familiya;
    group = students[idx].guruh;
    sid   = students[idx].sid;
    photoFile = students[idx].photo;
  }

  String action;
  if (!known) {
    action = "RUXSAT YO'Q";
    appendLogFile(nowString(), uid, "NOTANISH", "", "Ruxsat yo'q");
  } else if (!allowed) {
    action = "BLOKLANGAN";
    appendLogFile(nowString(), uid, name, group, "Bloklangan");
  } else {
    bool isIn = (lastState[uid] == "Kirdi");
    action = isIn ? "CHIQDI" : "KIRDI";
    lastState[uid] = isIn ? "Chiqdi" : "Kirdi";
    appendLogFile(nowString(), uid, name, group, isIn ? "Chiqdi" : "Kirdi");
  }

  bool cacheHit = (uid == cachedUid && photoBuf != nullptr && photoLen > 100);
  if (!cacheHit && known && photoFile.length()) {
    if (loadPhotoFromFS(photoFile)) cachedUid = uid;
  }
  if (!known) { photoLen = 0; if (photoBuf) { free(photoBuf); photoBuf = nullptr; } cachedUid = ""; }

  showResultPanel(uid, name, sid, group, known, allowed, action.c_str());
  Serial.printf("Natija: %s | ruxsat=%d | %s\n", name.c_str(), allowed, action.c_str());
}

void rfidLoop() {
  if (!rfidOk || cardShowing) return;
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;
  String uid = uidToString(rfid.uid);
  if (uid == lastUid && millis() - lastUidAt < 5000) {
    rfid.PICC_HaltA();
    return;
  }
  lastUid = uid;
  lastUidAt = millis();
  handleCard();
}

// =================== SETUP / LOOP ===================

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nINHA UNIVERSITY (mustaqil) ishga tushmoqda...");

  // Xotira
  if (!FFat.begin(true)) {
    Serial.println("FFat xato!");
  }
  if (!FFat.exists("/students.csv")) {
    File f = FFat.open("/photos/00B9B41A.jpg", "w");
    if (f) { f.write(bek_photo, bek_photo_len); f.close(); }
    f = FFat.open("/students.csv", "w");
    if (f) {
      f.println("00B9B41A,Bekmirza,Jumaboyev,S00101 (1-kurs),faol,00B9B41A.jpg");
      f.close();
    }
    Serial.println("Boshlang'ich o'quvchi qo'shildi");
  }
  loadStudents();
  seedStudents();
  patchCards();
  wipeOldData();
  loadLogs();

  // RFID
  rfidHardwareInit();

  // Ekran
  lcd.init();
  lcd.setRotation(0);

  // Touch kalibrovka (NVS dan yoki interaktiv)
  {
    uint16_t calData[8];
    prefs.begin("tftcal", true);
    size_t n = prefs.getBytes("calv2", calData, sizeof(calData));
    prefs.end();
    if (n == sizeof(calData)) {
      lcd.setTouchCalibrate(calData);
      Serial.println("Touch kalibrovka yuklandi");
    } else {
      lcd.fillScreen(TFT_BLACK);
      lcd.setTextDatum(textdatum_t::middle_center);
      lcd.setFont(&fonts::Font2);
      lcd.setTextColor(TFT_WHITE);
      lcd.drawString("Kalibrovka: nuqtalarni qalam bilan bosing", 160, 40);
      // Touch mavjudligini kutamiz (60s). Bo'lmasa — o'tkazib yuboramiz.
      bool hasTouch = false;
      int32_t px, py;
      uint32_t t0 = millis();
      while (millis() - t0 < 60000UL) {
        if (lcd.getTouch(&px, &py)) { hasTouch = true; break; }
        delay(60);
      }
      if (hasTouch) {
        delay(300);
        lcd.calibrateTouch(calData, 0xFFFF, 0x0000, 20);
        prefs.begin("tftcal", false);
        prefs.putBytes("calv2", calData, sizeof(calData));
        prefs.end();
        Serial.println("Touch kalibrovka saqlandi");
      } else {
        Serial.println("Touch topilmadi — kalibrovka o'tkazib yuborildi");
      }
    }
  }

  // Vaqtni NVS dan tiklash (dashboard sinxronlamaguncha)
  {
    prefs.begin("rtc", true);
    uint32_t ep = prefs.getUInt("epoch", 0);
    prefs.end();
    if (ep > 1600000000UL) {
      struct timeval tv;
      tv.tv_sec = (time_t)ep;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);
      clockSet = true;
      Serial.println("Vaqt NVS dan tiklandi");
    }
  }

  drawBackground();
  lastActivity = millis();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP: %s | IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  if (MDNS.begin("turniket")) MDNS.addService("http", "tcp", 80);

  // Web server
  server.on("/", handleRoot);
  server.on("/api/data", handleData);
  server.on("/api/time", handleTime);
  server.on("/api/del", handleDelete);
  server.on("/api/reset", handleReset);
  server.on("/api/export", handleExport);
  server.on("/add", HTTP_POST, handleAddDone, handleUpload);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server tayyor: http://192.168.4.1");

  drawCornerLabels();
  Serial.println("Tayyor!");
}

void loop() {
  server.handleClient();

  if (cardShowing && millis() - cardShownAt > CARD_HOLD_MS) {
    cardShowing = false;
    drawBackground();
  }

  if (!cardShowing) {
    // Sensor (touch) — uyg'otish va vaqtinchalik indikator
    int32_t tx, ty;
    if (lcd.getTouch(&tx, &ty)) {
      lastActivity = millis();
      if (sleeping) {
        wakeUp();
      } else {
        touching = true;
        drawTouchIndicator(tx, ty);
      }
    } else if (touching) {
      touching = false;
      drawBackground();
    }

    if (!sleeping) {
      updateClock();
      rfidLoop();
    } else {
      updateSleepClock();   // xira soatni yangilab turamiz
      rfidLoop();           // karta urilsa uyg'onadi
    }

    // Uyquga ketish (60s harakatsizlik)
    if (!sleeping && millis() - lastActivity > SLEEP_MS) {
      goSleep();
    }
  } else {
    lastActivity = millis();
  }

  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (!rfidOk) rfidHardwareInit();
    Serial.printf("Holat: RFID=%s | o'quvchi=%d | log=%d | ichida=%d | AP klient=%d | sleep=%d | heap=%u\n",
                  rfidOk ? "tayyor" : "topilmadi", (int)students.size(),
                  (int)logs.size(), insideCount(), WiFi.softAPgetStationNum(),
                  sleeping ? 1 : 0, (unsigned)ESP.getFreeHeap());
    if (!cardShowing && !rfidOk && !sleeping) drawCornerLabels();
  }

  delay(5);
}
