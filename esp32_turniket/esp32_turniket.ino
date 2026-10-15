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
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <map>
#include "logo_image.h"
#include "bek_photo.h"
#include "a9g.h"

// ---------- SOZLAMALAR ----------
#define AP_SSID  "INHA-TURNIKET"
#define AP_PASS  "turniket2026"

// ---------- PINLAR ----------
#define PIN_SCK     12
#define PIN_MOSI    11
#define PIN_LCD_CS  10
#define PIN_LCD_DC   8
#define PIN_LCD_RST  9

#define RFID_CS     15
#define RFID_SCK    16
#define RFID_MOSI   17
#define RFID_MISO   18
#define RFID_RST     4

#define CARD_HOLD_MS 4000

// ---------- IoT / A9G sozlamalari ----------
#define NET_ENABLED     1
#define NET_SERVER_HOST "kilometers-modes-interracial-comparing.trycloudflare.com"   // standart (FFat /server.txt ustidan yozadi)
#define NET_SERVER_PORT 80
#define NET_UPLOAD_MS   60000                    // har 1 daqiqada yuborish

// ---------- DISPLAY ----------
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel;
  lgfx::Bus_SPI       _bus;
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
      cfg.pin_miso    = -1;
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
      cfg.readable         = false;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

LGFX lcd;
WebServer server(80);
MFRC522 rfid(RFID_CS, RFID_RST);

bool rfidOk = false, rfidSwapBuses = false;
bool cardShowing = false;
SemaphoreHandle_t fsMutex = nullptr;   // FFat uchun (ikki yadro birga ishlaydi)

// ---- Kartaga yozish rejimi ----
bool writeMode = false;
String writeUid = "", writeName = "", writeData = "";
uint32_t writeModeAt = 0;

// Lokal qo'shilgan o'quvchilar (serverdan o'chirib yuborilmasligi uchun)
struct RecentAdd { String uid; uint32_t at; };
std::vector<RecentAdd> recentAdds;
static bool recentlyAdded(const String& uid) {
  for (auto& r : recentAdds)
    if (r.uid.equalsIgnoreCase(uid) && millis() - r.at < 180000) return true;
  return false;
}

// Yuklab bo'lmagan rasmlar (qayta urinishni kamaytirish uchun)
struct PhotoFail { String name; uint32_t at; uint32_t dur; };
std::vector<PhotoFail> photoFails;
static bool photoRecentlyFailed(const String& fname) {
  for (auto& pf : photoFails)
    if (pf.name == fname && millis() - pf.at < pf.dur) return true;
  return false;
}
uint32_t cardShownAt = 0;
uint32_t lastClock = UINT32_MAX;
String lastUid = "";
uint32_t lastUidAt = 0;

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
  // IoT navbati: serverga yuborilishi kerak bo'lgan hodisalar
  if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
  File n = FFat.open("/netq.csv", "a");
  if (n) {
    n.printf("%s,%s,%s,%s,%s\n", ts.c_str(), uid.c_str(), ism.c_str(), guruh.c_str(), holat.c_str());
    n.close();
  }
  if (fsMutex) xSemaphoreGive(fsMutex);
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
    if (clean.length() > 8) clean = clean.substring(0, 8);   // FFat 8.3 nom cheklovi
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
  recentAdds.push_back(RecentAdd{uid, millis()});
  if (recentAdds.size() > 30) recentAdds.erase(recentAdds.begin());
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

// Kartaga yozish rejimini yoqish: /api/write?uid=XXX  (yoki ?cancel=1)
void handleWriteSet() {
  if (server.hasArg("cancel")) {
    writeMode = false;
    if (!cardShowing) drawBackground();
    server.send(200, "application/json", "{\"ok\":1,\"mode\":\"off\"}");
    return;
  }
  String uid = server.arg("uid");
  uid.trim(); uid.toUpperCase();
  int idx = -1;
  for (size_t i = 0; i < students.size(); i++) {
    if (uidMatches(students[i].uid, uid)) { idx = (int)i; break; }
  }
  if (idx < 0) {
    server.send(404, "application/json", "{\"error\":\"topilmadi\"}");
    return;
  }
  writeUid = uid;
  writeName = students[idx].ism + " " + students[idx].familiya;
  writeData = "INHA|" + students[idx].ism + "|" + students[idx].familiya + "|" +
              students[idx].sid + "|" + students[idx].guruh;
  if (writeData.length() > 47) writeData = writeData.substring(0, 47);
  writeMode = true;
  writeModeAt = millis();
  drawWritePrompt();
  Serial.println("[Yozish] rejim yoqildi: " + writeName);
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
}

void drawBackground() {
  lcd.drawJpg(logo_jpg, logo_jpg_len, 0, 0);
  drawCornerLabels();
  drawNetStatus();
  drawHint();
  lcd.fillRect(226, 4, 90, 20, BG_BLUE);
  lastClock = UINT32_MAX;
  updateClock(true);
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

// ---- Kartaga ma'lumot yozish (MIFARE sector 1, bloklar 4-6) ----
bool writeCardData(const String& data) {
  uint8_t buf[48];
  memset(buf, 0, sizeof(buf));
  int n = data.length();
  if (n > 47) n = 47;
  memcpy(buf, data.c_str(), n);

  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;   // standart kalit

  for (byte block = 4; block <= 6; block++) {
    MFRC522::StatusCode st = rfid.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
    if (st != MFRC522::STATUS_OK) {
      Serial.printf("[Yozish] auth xato blok %d: %d\n", block, (int)st);
      return false;
    }
    st = rfid.MIFARE_Write(block, &buf[(block - 4) * 16], 16);
    if (st != MFRC522::STATUS_OK) {
      Serial.printf("[Yozish] yozish xato blok %d: %d\n", block, (int)st);
      return false;
    }
  }
  return true;
}

void drawWritePrompt() {
  lcd.fillRoundRect(8, 36, 304, 408, 14, PANEL);
  lcd.drawRoundRect(8, 36, 304, 408, 14, YEL);
  lcd.drawRoundRect(11, 39, 298, 402, 12, YEL);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(YEL, PANEL);
  lcd.drawString("KARTA YOZISH", 160, 90);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE, PANEL);
  lcd.drawString(writeName, 160, 160);
  lcd.setTextColor(TFT_CYAN, PANEL);
  lcd.drawString("Kartani taqdim eting", 160, 220);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(0xC618, PANEL);
  lcd.drawString("Bekor qilish: /api/write?cancel=1", 160, 300);
}

void drawWriteResult(const String& cardUid, bool ok) {
  cardShowing = true;
  cardShownAt = millis();
  uint16_t color = ok ? GRN : RED;
  lcd.fillRoundRect(8, 36, 304, 408, 14, PANEL);
  lcd.drawRoundRect(8, 36, 304, 408, 14, color);
  lcd.drawRoundRect(11, 39, 298, 402, 12, color);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(color, PANEL);
  lcd.drawString(ok ? "YOZILDI!" : "XATO!", 160, 110);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE, PANEL);
  lcd.drawString(writeName, 160, 180);
  lcd.setTextColor(YEL, PANEL);
  lcd.drawString("Karta: " + cardUid, 160, 220);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(0xC618, PANEL);
  if (!ok) lcd.drawString("Karta mos emas yoki kalit boshqa", 160, 260);
  lcd.drawString("Kartani olishingiz mumkin", 160, 290);
}

void handleCardWrite() {
  String cardUid = uidToString(rfid.uid);
  byte sak = rfid.uid.sak;
  String tname = rfid.PICC_GetTypeName(rfid.PICC_GetType(sak));
  Serial.printf("[Yozish] Karta turi: %s (SAK=0x%02X)\n", tname.c_str(), sak);
  bool ok = writeCardData(writeData);
  Serial.printf("[Yozish] %s -> %s\n", cardUid.c_str(), ok ? "OK" : "XATO");
  writeMode = false;
  drawWriteResult(cardUid, ok);
  rfid.PICC_HaltA();
}


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

  if (writeMode) {
    handleCardWrite();
    return;
  }

  String uid = uidToString(rfid.uid);
  if (uid == lastUid && millis() - lastUidAt < 2000) {
    rfid.PICC_HaltA();
    return;
  }
  lastUid = uid;
  lastUidAt = millis();
  handleCard();
}

// =================== IoT: SERVERGA YUBORISH ===================

String netHost = NET_SERVER_HOST;   // FFat /server.txt dan o'qilishi mumkin
volatile bool netModem = false, netSim = false, netReg = false, netGprs = false;
volatile int  netCsq = 0;
volatile int  uiNetState = 0;      // 0=bo'sh, 1=yuborilmoqda, 2=yuborildi, 3=xato
volatile uint32_t uiNetAt = 0;
uint32_t pushCount = 0;            // serverga yuborilgan o'quvchi sinxronlar soni

String urlEnc(const String& s) {
  String r = "";
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      r += (char)c;
    } else {
      r += '%'; r += hex[c >> 4]; r += hex[c & 0xF];
    }
  }
  return r;
}

void processQueue() {
#if NET_ENABLED
  if (netHost.length() == 0) return;   // server hali sozlanmagan

  std::vector<String> lines;
  if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
  File f = FFat.open("/netq.csv", "r");
  if (f) {
    while (f.available()) {
      String l = f.readStringUntil('\n');
      l.trim();
      if (l.length() > 4) lines.push_back(l);
    }
    f.close();
  }
  if (fsMutex) xSemaphoreGive(fsMutex);

  if (!lines.empty()) {
    // Juda uzun navbat bo'lsa eskilarini tashlaymiz
    if (lines.size() > 500) lines.erase(lines.begin(), lines.begin() + (lines.size() - 500));

    Serial.printf("[IoT] Navbatda %d ta hodisa\n", (int)lines.size());
    uiNetState = 1;
    if (!a9gEnsureOnline()) {
      Serial.println("[IoT] Aloqa yo'q - keyingi urinishda qayta yuboramiz");
      uiNetState = 3;
      uiNetAt = millis();
    } else {
      size_t sent = 0;
      for (auto& line : lines) {
        String p[5];
        int idx = 0, start = 0;
        for (int i = 0; i <= (int)line.length() && idx < 5; i++) {
          if (i == (int)line.length() || line[i] == ',') {
            p[idx++] = line.substring(start, i);
            start = i + 1;
          }
        }
        if (idx < 5) continue;

        String path = "/api/event?ts=" + urlEnc(p[0]) + "&uid=" + urlEnc(p[1]) +
                      "&ism=" + urlEnc(p[2]) + "&guruh=" + urlEnc(p[3]) + "&holat=" + urlEnc(p[4]);
        String resp;
        if (a9gHttpGet(netHost.c_str(), NET_SERVER_PORT, path, resp) && resp.indexOf("200") >= 0) {
          sent++;
          Serial.printf("[IoT] Yuborildi: %s | %s | %s\n", p[0].c_str(), p[1].c_str(), p[4].c_str());
          vTaskDelay(pdMS_TO_TICKS(200));
        } else {
          Serial.println("[IoT] Yuborishda xato - keyingi siklda qayta urinamiz");
          break;
        }
      }

      if (sent > 0) {
        if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
        File w = FFat.open("/netq.csv", "w");
        if (w) {
          for (size_t i = sent; i < lines.size(); i++) w.println(lines[i]);
          w.close();
        }
        if (fsMutex) xSemaphoreGive(fsMutex);
        Serial.printf("[IoT] Jami yuborildi: %d ta\n", (int)sent);
      }
      uiNetState = 2;
      uiNetAt = millis();
    }
  }

  // O'quvchilar sinxronizatsiyasi (navbat bo'sh bo'lsa — har 60 sekundda 1 tadan)
  static uint32_t lastPush = 0;
  if (lines.empty() && a9gGprsUp && !students.empty() && millis() - lastPush > 60000) {
    lastPush = millis();
    static size_t syncIdx = 0;
    Student& st = students[syncIdx % students.size()];
    syncIdx++;
    String path = "/api/student?uid=" + urlEnc(st.uid) + "&ism=" + urlEnc(st.ism) +
                  "&familiya=" + urlEnc(st.familiya) + "&sid=" + urlEnc(st.sid) +
                  "&guruh=" + urlEnc(st.guruh) + "&holat=" + urlEnc(st.holat);
    String resp;
    uiNetState = 1;
    uiNetAt = millis();
    if (a9gHttpGet(netHost.c_str(), NET_SERVER_PORT, path, resp) && resp.indexOf("200") >= 0) {
      Serial.println("[IoT] O'quvchi sinxron: " + st.ism);
      pushCount++;
      uiNetState = 2;
    } else {
      uiNetState = 0;
    }
    uiNetAt = millis();
  }
#endif
}

// =================== IoT: SERVERDAN OLISH (o'quvchi + rasm) ===================

uint32_t studentsRev = 0;

void syncStudentsFromServer() {
  if (!a9gModemFound) return;
  if (!a9gGprsUp && !a9gEnsureOnline()) return;

  String resp;
  String path = String("/api/students/tsv?rev=") + studentsRev;
  uiNetState = 1; uiNetAt = millis();
  if (!a9gHttpGet(netHost.c_str(), NET_SERVER_PORT, path, resp)) { uiNetState = 0; return; }

  int ri = resp.indexOf("REV ");
  if (ri < 0) { uiNetState = 0; return; }
  long rev = resp.substring(ri + 4).toInt();
  int nl = resp.indexOf('\n', ri);
  int changed = 0;
  bool revChanged = (rev != (long)studentsRev);
  std::vector<String> seenUids;

  if (nl > 0) {
    int pos = nl + 1;
    while (pos < (int)resp.length()) {
      int e = resp.indexOf('\n', pos);
      String line = (e < 0) ? resp.substring(pos) : resp.substring(pos, e);
      pos = (e < 0) ? resp.length() : e + 1;
      line.trim();
      if (line.length() < 3 || line.startsWith("CLOSED") || line.startsWith("OK")) continue;

      String p[7];
      int idx = 0, st = 0;
      for (int i = 0; i <= (int)line.length() && idx < 7; i++) {
        if (i == (int)line.length() || line[i] == '|') { p[idx++] = line.substring(st, i); st = i + 1; }
      }
      while (idx < 7) p[idx++] = "";
      String uid = p[0]; uid.trim();
      if (!uid.length()) continue;
      // UID tekshiruvi: faqat hex belgilar va ';' (buzilgan qatorlardan himoya)
      bool uidOk = true;
      for (unsigned k = 0; k < uid.length(); k++) {
        char c = uid[k];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
              (c >= 'a' && c <= 'f') || c == ';')) { uidOk = false; break; }
      }
      if (!uidOk) continue;
      // Rasm nomi tekshiruvi: faqat .jpg yoki bo'sh
      String photoName = p[6];
      if (photoName.length() && !photoName.endsWith(".jpg")) photoName = "";
      seenUids.push_back(uid);

      bool found = false;
      for (auto& s : students) {
        if (s.uid.equalsIgnoreCase(uid)) {
          if (s.ism != p[1] || s.familiya != p[2] || s.sid != p[3] ||
              s.guruh != p[4] || s.holat != p[5]) {
            s.ism = p[1]; s.familiya = p[2]; s.sid = p[3];
            s.guruh = p[4]; s.holat = p[5];
            changed++;
          }
          if (photoName.length() && s.photo != photoName) {
            String oldPhoto = s.photo;
            s.photo = photoName;
            changed++;
            if (oldPhoto.length() && oldPhoto != photoName) {
              if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
              FFat.remove("/photos/" + oldPhoto);
              if (fsMutex) xSemaphoreGive(fsMutex);
            }
            if (cachedUid.equalsIgnoreCase(uid)) cachedUid = "";   // keshni yangilash
          }
          found = true;
          break;
        }
      }
      if (!found) {
        students.push_back(Student{uid, p[1], p[2], p[3], p[4],
                                   p[5].length() ? p[5] : "faol", photoName});
        changed++;
      }
    }
  }
  // Serverda yo'q bo'lganlarni lokal ro'yxatdan ham o'chiramiz
  // (xavfsizlik: barcha lokal o'quvchilar kamida bir marta serverga yuborilgach)
  if (revChanged && seenUids.size() >= 3 && pushCount >= students.size()) {
    for (int i = (int)students.size() - 1; i >= 0; i--) {
      bool seen = false;
      for (auto& u : seenUids) {
        if (students[i].uid.equalsIgnoreCase(u)) { seen = true; break; }
      }
      if (!seen && !recentlyAdded(students[i].uid)) {
        students.erase(students.begin() + i);
        changed++;
      }
    }
  }
  if (studentsRev != rev) studentsRev = rev;
  if (changed > 0) {
    if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
    saveStudents();
    if (fsMutex) xSemaphoreGive(fsMutex);
    Serial.printf("[IoT] Serverdan yangilandi: %d ta o'zgarish\n", changed);
  }
  uiNetState = 2; uiNetAt = millis();
}

// Bufer ichidan matn izlash
static bool bufContains(const uint8_t* b, size_t n, const char* pat) {
  size_t pl = strlen(pat);
  if (n < pl) return false;
  for (size_t i = 0; i + pl <= n; i++) {
    if (b[i] == (uint8_t)pat[0] && memcmp(b + i, pat, pl) == 0) return true;
  }
  return false;
}

// Serverdan rasmni yuklab, FFat ga saqlash (JPEG) — PSRAM bufer bilan
int downloadPhoto(const String& fname, const String& savePath) {
  if (!fname.length()) return 0;
  Serial.println("[IoT] Rasm yuklanmoqda: " + fname);

  const size_t CAP = 120000;
  uint8_t* buf = (uint8_t*)ps_malloc(CAP);
  if (!buf) buf = (uint8_t*)malloc(CAP);
  if (!buf) { Serial.println("[IoT] Rasm: xotira yo'q"); return -1; }
  size_t blen = 0;

  a9gCmd("AT+CIPCLOSE", 2000, "OK");
  a9gCmd("AT+CIPMUX=0", 2000, "OK");

  static String photoIP = "";
  if (photoIP.length() == 0) {
    String dns = a9gCmd(String("AT+CDNSGIP=\"") + netHost + "\"", 15000, "+CDNSGIP");
    int q1 = dns.indexOf("\",\"");
    if (dns.indexOf("+CDNSGIP: 1") >= 0 && q1 >= 0) {
      int q2 = dns.indexOf("\"", q1 + 3);
      if (q2 > q1) photoIP = dns.substring(q1 + 3, q2);
    }
  }
  String target = photoIP.length() ? photoIP : netHost;
  vTaskDelay(pdMS_TO_TICKS(1500));   // oldingi ulanish to'liq yopilishini kutish
  String r;
  for (int a = 0; a < 3; a++) {
    r = a9gCmd(String("AT+CIPSTART=\"TCP\",\"") + target + "\",8080", 15000, "CONNECT OK");
    if (r.indexOf("CONNECT OK") >= 0) break;
    vTaskDelay(pdMS_TO_TICKS(2500));
    a9gCmd("AT+CIPCLOSE", 2000, "OK");
  }
  if (r.indexOf("CONNECT OK") < 0) {
    Serial.println("[IoT] Rasm: TCP ulanmadi");
    photoIP = "";
    free(buf);
    return -1;
  }

  String req = "GET /photos/" + fname + " HTTP/1.1\r\nHost: " + netHost +
               "\r\nUser-Agent: inha-turniket\r\nConnection: close\r\n\r\n";
  while (a9gSerial.available()) a9gSerial.read();
  a9gSerial.print(String("AT+CIPSEND=") + req.length() + "\r\n");
  bool prompt = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    while (a9gSerial.available()) { if (a9gSerial.read() == '>') { prompt = true; break; } }
    if (prompt) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!prompt) {
    Serial.println("[IoT] Rasm: CIPSEND prompt yo'q");
    free(buf);
    return -1;
  }
  a9gSerial.print(req);

  t0 = millis();
  uint32_t tProgress = millis();
  while (millis() - t0 < 110000) {
    while (a9gSerial.available() && blen < CAP) buf[blen++] = (uint8_t)a9gSerial.read();
    if (bufContains(buf, blen, "CLOSED")) break;
    if (blen >= CAP) break;
    if (millis() - tProgress > 10000) {
      tProgress = millis();
      Serial.printf("[IoT] Rasm: %u bayt olinmoqda...\n", (unsigned)blen);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  Serial.printf("[IoT] Rasm: oqim tugadi, %u bayt\n", (unsigned)blen);
  a9gCmd("AT+CIPCLOSE", 2000, "OK");

  // +CIPRCV,NNN: markerlarini joyida tozalaymiz
  size_t w = 0;
  for (size_t i = 0; i < blen; i++) {
    if (i + 8 < blen && buf[i] == '+' && memcmp(buf + i, "+CIPRCV,", 8) == 0) {
      size_t j = i + 8;
      while (j < blen && buf[j] >= '0' && buf[j] <= '9') j++;
      if (j < blen && buf[j] == ':') { i = j; continue; }
    }
    buf[w++] = buf[i];
  }
  blen = w;

  // JPEG: SOI (FFD8) ... EOI (FFD9)
  long soi = -1, eoi = -1;
  for (size_t i = 0; i + 1 < blen; i++) {
    if (buf[i] == 0xFF && buf[i + 1] == 0xD8) { soi = (long)i; break; }
  }
  for (long i = (long)blen - 2; i > 0; i--) {
    if (buf[i] == 0xFF && buf[i + 1] == 0xD9) { eoi = i + 1; break; }
  }
  if (soi < 0 || eoi <= soi || (eoi - soi) < 500) {
    bool isHttp = bufContains(buf, blen, "HTTP/");
    Serial.printf("[IoT] Rasm xato (%s): %u bayt, http=%d, soi=%ld eoi=%ld\n",
                  fname.c_str(), (unsigned)blen, isHttp ? 1 : 0, soi, eoi);
    free(buf);
    return (blen < 2000) ? 0 : -1;     // 0 = doimiy xato (404), -1 = tarmoq
  }

  if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
  if (!FFat.exists("/photos")) FFat.mkdir("/photos");
  File f = FFat.open(savePath, "w");
  bool ok = false;
  if (f) {
    f.write(buf + soi, eoi - soi + 1);
    f.close();
    ok = true;
  }
  if (fsMutex) xSemaphoreGive(fsMutex);
  free(buf);
  if (ok) Serial.printf("[IoT] Rasm yuklandi: %s (%ld bayt)\n", fname.c_str(), eoi - soi + 1);
  return ok ? 1 : -1;
}

// =================== IoT: TARMOQ HOLATI (ekranda) ===================

void drawNetStatus() {
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font0);
  lcd.fillRect(6, 70, 170, 14, BG_BLUE);

  char b[16];
  snprintf(b, sizeof(b), "GSM:%d", (int)netCsq);
  lcd.setTextColor(netGprs ? GRN : TFT_WHITE, BG_BLUE);
  lcd.drawString(b, 6, 70);

  int level = netModem ? ((int)netCsq * 5 + 15) / 31 : 0;
  for (int i = 0; i < 5; i++) {
    int h = 3 + i * 2;
    uint16_t c = (i < level) ? (netGprs ? GRN : YEL) : 0x4208;
    lcd.fillRect(50 + i * 8, 80 - h, 5, h, c);
  }

  const char* st;
  uint16_t col;
  if (netGprs)       { st = "NET:onlayn";   col = GRN; }
  else if (netReg)   { st = "NET:tarmoq";   col = YEL; }
  else if (netSim)   { st = "NET:qidiruv";  col = YEL; }
  else if (netModem) { st = "NET:SIM yo'q"; col = RED; }
  else               { st = "NET:modem yo'q"; col = RED; }
  lcd.setTextColor(col, BG_BLUE);
  lcd.drawString(st, 96, 70);
}

// Yuborish animatsiyasi (asosiy loopdan chaqiriladi — LCD faqat bitta vazifada)
void drawSyncStatus() {
  int st = uiNetState;
  uint32_t ago = millis() - uiNetAt;
  if (st == 2 && ago > 4000) st = 0;
  if (st == 3 && ago > 6000) st = 0;

  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setFont(&fonts::Font0);
  lcd.fillRect(6, 86, 176, 12, BG_BLUE);
  if (st == 1) {
    int dots = (millis() / 300) % 4;
    char b[28];
    snprintf(b, sizeof(b), "YUBORILMOQDA%.*s", dots, "...");
    lcd.setTextColor(YEL, BG_BLUE);
    lcd.drawString(b, 6, 86);
  } else if (st == 2) {
    lcd.setTextColor(GRN, BG_BLUE);
    lcd.drawString("YUBORILDI OK", 6, 86);
  } else if (st == 3) {
    lcd.setTextColor(RED, BG_BLUE);
    lcd.drawString("ALOQA YO'Q - navbatda", 6, 86);
  }
}

// Modem holatini tekshirish (faqat tarmoq taskidan chaqiriladi)
void netStatusCheck() {
  a9gCmd("ATE0", 1000, "OK");
  String csq = a9gCmd("AT+CSQ", 2000, "+CSQ");
  int ci = csq.indexOf("+CSQ:");
  int v = (ci >= 0) ? csq.substring(ci + 5).toInt() : 0;
  if (v > 31) v = 0;
  netCsq = v;

  String cpin = a9gCmd("AT+CPIN?", 2000, "+CPIN");
  netSim = cpin.indexOf("READY") >= 0;

  String creg = a9gCmd("AT+CREG?", 2000, "+CREG");
  netReg = netSim && (creg.indexOf(",1") >= 0 || creg.indexOf(",5") >= 0);
  netModem = a9gModemFound;
  netGprs = a9gGprsUp;
}

// ===== TARMOQ TASKI (alohida yadro — karta o'qishni bloklamaydi) =====
static void netTaskCode(void*) {
  vTaskDelay(pdMS_TO_TICKS(12000));   // tizim ishga tushishini kutamiz
  uint8_t fails = 0;
  uint32_t lastPulse = 0;
  uint32_t cycle = 0;
  uint8_t pullCycle = 0;
  bool timeSynced = false;
  for (;;) {
    if (strlen(netHost.c_str()) > 0) {
      cycle++;
      if (!a9gModemFound) {
        a9gModemFound = a9gFind();
        if (!a9gModemFound) {
          fails++;
          if (fails >= 3 && millis() - lastPulse > 180000) {
            lastPulse = millis();
            fails = 0;
            a9gPowerPulse();
            vTaskDelay(pdMS_TO_TICKS(8000));
            a9gModemFound = a9gFind();
          }
          netSim = netReg = netGprs = false;
          netCsq = 0;
        } else {
          fails = 0;
        }
      }
      if (a9gModemFound) {
        // Avval serverdagi yangilanishlarni OLAMIZ (web orqali tahrirlangan bo'lishi mumkin)
        if (++pullCycle >= 4) {
          pullCycle = 0;
          syncStudentsFromServer();
          // Yetishmayotgan rasmni yuklab olamiz (har siklda ko'pi bilan 1 ta)
          for (auto& s : students) {
            if (s.photo.length()) {
              if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY);
              bool exists = FFat.exists("/photos/" + s.photo);
              if (fsMutex) xSemaphoreGive(fsMutex);
              if (!exists && !photoRecentlyFailed(s.photo)) {
                uiNetState = 1; uiNetAt = millis();
                int st = downloadPhoto(s.photo, "/photos/" + s.photo);
                if (st != 1) {
                  uint32_t dur = (st == 0) ? 1800000 : 300000;   // 404: 30daq, tarmoq: 5daq
                  photoFails.push_back(PhotoFail{s.photo, millis(), dur});
                  if (photoFails.size() > 40) photoFails.erase(photoFails.begin());
                }
                uiNetState = 2; uiNetAt = millis();
                break;
              }
            }
          }
        }
        processQueue();               // navbat: yuborish + o'quvchi sinxron (PUSH)
        // Vaqtni serverdan sinxronlash (bir marta; GPRS ni ham ko'taradi)
        if (!timeSynced) {
          if (a9gEnsureOnline()) {
            String tresp;
            if (a9gHttpGet(netHost.c_str(), NET_SERVER_PORT, "/api/now", tresp) &&
                tresp.indexOf("\"ms\":") >= 0) {
              long long msVal = atoll(tresp.substring(tresp.indexOf("\"ms\":") + 5).c_str());
              if (msVal > 1600000000000LL) {
                time_t secs = (time_t)(msVal / 1000);
                struct timeval tv = {secs, 0};
                settimeofday(&tv, nullptr);
                clockSet = true;
                lastClock = UINT32_MAX;
                timeSynced = true;
                Serial.println("[IoT] Vaqt serverdan sinxronlandi");
              }
            }
          }
        }
        if (cycle % 3 == 0) netStatusCheck();
      }
      netModem = a9gModemFound;
      netGprs = a9gGprsUp;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));  // har 5 sekundda
  }
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
  if (!FFat.exists("/photos")) FFat.mkdir("/photos");   // rasmlar papkasi (majburiy!)
  // Server manzili (tunnel URL) — FFat /server.txt dan o'qiladi
  netHost = NET_SERVER_HOST;
  {
    File sf = FFat.open("/server.txt", "r");
    if (sf) {
      String h = sf.readStringUntil('\n');
      h.trim();
      if (h.length() > 5) netHost = h;
      sf.close();
    }
  }
  Serial.println("Server manzil: " + netHost);
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
  // Eski buzuq rasm nomlarini tozalash (8.3/parse xatolaridan himoya)
  {
    bool fixedAny = false;
    for (auto& s : students) {
      if (s.photo.length() && !s.photo.endsWith(".jpg")) { s.photo = ""; fixedAny = true; }
    }
    if (fixedAny) { saveStudents(); Serial.println("Buzilgan rasm nomlari tozalandi"); }
  }
  seedStudents();
  patchCards();
  wipeOldData();
  loadLogs();

  // RFID
  rfidHardwareInit();

  // Ekran
  lcd.init();
  lcd.setRotation(0);
  drawBackground();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // quvvat tejamkorlik: cho'qqi tokni kamaytiradi
  Serial.printf("AP: %s | IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  if (MDNS.begin("turniket")) MDNS.addService("http", "tcp", 80);

  // Web server
  server.on("/", handleRoot);
  server.on("/api/data", handleData);
  server.on("/api/time", handleTime);
  server.on("/api/del", handleDelete);
  server.on("/api/reset", handleReset);
  server.on("/api/write", handleWriteSet);
  server.on("/api/export", handleExport);
  server.on("/api/sethost", []() {
    String h = server.arg("host");
    h.trim();
    if (h.length() > 5) {
      File w = FFat.open("/server.txt", "w");
      if (w) { w.println(h); w.close(); }
      netHost = h;
      Serial.println("[IoT] Yangi server manzil: " + h);
      server.send(200, "application/json", "{\"ok\":1}");
    } else {
      server.send(400, "application/json", "{\"error\":\"host kerak\"}");
    }
  });
  server.on("/add", HTTP_POST, handleAddDone, handleUpload);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server tayyor: http://192.168.4.1");

  // Tarmoq taskini alohida yadroda ishga tushirish (karta o'qishni bloklamaydi)
  fsMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(netTaskCode, "netTask", 10240, nullptr, 1, nullptr, 0);

  drawCornerLabels();
  Serial.println("Tayyor!");
}

void loop() {
  server.handleClient();

  // USB Serial buyruqlari (avtonom: tunnel URL ni masofadan yangilash)
  static String cmdBuf = "";
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      cmdBuf.trim();
      if (cmdBuf.startsWith("SETHOST=")) {
        String h = cmdBuf.substring(8);
        h.trim();
        if (h.length() > 5) {
          netHost = h;
          File w = FFat.open("/server.txt", "w");
          if (w) { w.println(h); w.close(); }
          Serial.println("[IoT] Server manzil yangilandi: " + h);
        }
      } else if (cmdBuf.startsWith("WRITE=")) {
        String uid = cmdBuf.substring(6);
        uid.trim(); uid.toUpperCase();
        if (uid == "OFF") {
          writeMode = false;
          if (!cardShowing) drawBackground();
          Serial.println("[Yozish] bekor qilindi");
        } else {
          int idx = -1;
          for (size_t i = 0; i < students.size(); i++) {
            if (uidMatches(students[i].uid, uid)) { idx = (int)i; break; }
          }
          if (idx >= 0) {
            writeUid = uid;
            writeName = students[idx].ism + " " + students[idx].familiya;
            writeData = "INHA|" + students[idx].ism + "|" + students[idx].familiya + "|" +
                        students[idx].sid + "|" + students[idx].guruh;
            if (writeData.length() > 47) writeData = writeData.substring(0, 47);
            writeMode = true;
            writeModeAt = millis();
            drawWritePrompt();
            Serial.println("[Yozish] rejim: " + writeName + " — kartani tuting");
          } else {
            Serial.println("[Yozish] o'quvchi topilmadi: " + uid);
          }
        }
      } else if (cmdBuf == "NET?") {
        Serial.println("[IoT] host=" + netHost + " | " +
                       (netGprs ? "onlayn" : (netReg ? "tarmoq" : (netSim ? "qidiruv" : "aloqa yo'q"))));
      }
      cmdBuf = "";
    } else if (cmdBuf.length() < 120) {
      cmdBuf += c;
    }
  }

  if (cardShowing && millis() - cardShownAt > CARD_HOLD_MS) {
    cardShowing = false;
    drawBackground();
  }

  // Yozish rejimi 2 daqiqadan keyin avtomatik bekor bo'ladi
  if (writeMode && millis() - writeModeAt > 120000) {
    writeMode = false;
    if (!cardShowing) drawBackground();
  }

  // Yuborish animatsiyasi (LCD faqat shu yerda chiziladi)
  static uint32_t lastUiDraw = 0;
  if (!cardShowing && millis() - lastUiDraw > 300) {
    lastUiDraw = millis();
    drawSyncStatus();
  }

  if (!cardShowing) {
    updateClock();
    rfidLoop();
  }

  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 15000) {
    lastCheck = millis();
    if (!rfidOk) rfidHardwareInit();
    Serial.printf("Holat: RFID=%s | o'quvchi=%d | log=%d | ichida=%d | AP klient=%d | net=%s csq=%d | heap=%u\n",
                  rfidOk ? "tayyor" : "topilmadi", (int)students.size(),
                  (int)logs.size(), insideCount(), WiFi.softAPgetStationNum(),
                  netGprs ? "onlayn" : (netReg ? "tarmoq" : (netSim ? "qidiruv" : (netModem ? "sim-yoq" : "modem-yoq"))),
                  (int)netCsq, (unsigned)ESP.getFreeHeap());
    if (!cardShowing) drawNetStatus();
    if (!cardShowing && !rfidOk) drawCornerLabels();
  }

  delay(5);
}
