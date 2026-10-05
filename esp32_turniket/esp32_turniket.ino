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

#define CARD_HOLD_MS 7000

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

  // Bugun kirdi / chiqdi ro'yxatlari
  j += ",\"kirdiList\":[";
  { bool f2 = true; int cnt = 0;
    for (int i = (int)logs.size() - 1; i >= 0 && cnt < 40; i--) {
      auto& e = logs[i];
      if (e.holat != "Kirdi") continue;
      if (clockSet && !e.ts.startsWith(today)) continue;
      String sid = "";
      for (auto& s : students) if (uidMatches(s.uid, e.uid)) { sid = s.sid; break; }
      if (!f2) j += ","; f2 = false; cnt++;
      j += "{\"ts\":\"" + jsonEsc(e.ts) + "\",\"ism\":\"" + jsonEsc(e.ism) +
           "\",\"familiya\":\"\",\"sid\":\"" + jsonEsc(sid) + "\",\"holat\":\"Kirdi\"}";
    }
  }
  j += "],\"chiqdiList\":[";
  { bool f2 = true; int cnt = 0;
    for (int i = (int)logs.size() - 1; i >= 0 && cnt < 40; i--) {
      auto& e = logs[i];
      if (e.holat != "Chiqdi") continue;
      if (clockSet && !e.ts.startsWith(today)) continue;
      String sid = "";
      for (auto& s : students) if (uidMatches(s.uid, e.uid)) { sid = s.sid; break; }
      if (!f2) j += ","; f2 = false; cnt++;
      j += "{\"ts\":\"" + jsonEsc(e.ts) + "\",\"ism\":\"" + jsonEsc(e.ism) +
           "\",\"familiya\":\"\",\"sid\":\"" + jsonEsc(sid) + "\",\"holat\":\"Chiqdi\"}";
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
  logs.clear();
  lastState.clear();
  server.send(200, "application/json", "{\"ok\":1}");
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
  lcd.drawString("Kartani taqdim eting", 240, 314);
}

void updateClock(bool force = false) {
  uint32_t now = millis() / 1000;
  if (!force && now == lastClock) return;
  lastClock = now;
  lcd.fillRect(340, 4, 136, 20, BG_BLUE);
  lcd.setTextDatum(textdatum_t::top_right);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString(clockShort(), 474, 4);
}

void drawBackground() {
  lcd.drawJpg(logo_jpg, logo_jpg_len, 0, 0);
  drawCornerLabels();
  drawHint();
  lcd.fillRect(340, 4, 136, 20, BG_BLUE);
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

  lcd.fillRoundRect(20, 30, 440, 270, 16, PANEL);
  lcd.drawRoundRect(20, 30, 440, 270, 16, color);
  lcd.drawRoundRect(23, 33, 434, 264, 14, color);

  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(color, PANEL);
  lcd.drawString(allowed ? "XUSH KELIBSIZ!" : "RUXSAT YO'Q", 240, 58);

  lcd.drawRect(45, 95, 160, 160, 0x8410);
  if (photoLen > 100) {
    lcd.drawJpg(photoBuf, photoLen, 46, 96, 158, 158);
  } else {
    lcd.fillRect(46, 96, 158, 158, 0x18E3);
    lcd.setFont(&fonts::Font0);
    lcd.setTextColor(0xC618, 0x18E3);
    lcd.drawString(known ? "RASM YO'Q" : "NOTANISH", 125, 175);
  }

  lcd.setTextDatum(textdatum_t::middle_left);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(TFT_WHITE, PANEL);
  char buf[64];
  if (known) {
    snprintf(buf, sizeof(buf), "%s", name.c_str());
    if (strlen(buf) > 22) { buf[20] = '.'; buf[21] = '.'; buf[22] = '.'; buf[23] = 0; }
  } else {
    snprintf(buf, sizeof(buf), "Karta bazada yo'q");
  }
  lcd.drawString(buf, 225, 115);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(YEL, PANEL);
  snprintf(buf, sizeof(buf), "Talaba ID: %s", sid.length() ? sid.c_str() : "-");
  lcd.drawString(buf, 225, 148);

  lcd.setTextColor(TFT_CYAN, PANEL);
  snprintf(buf, sizeof(buf), "Guruh: %s", group.length() ? group.c_str() : "-");
  lcd.drawString(buf, 225, 170);

  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(0xC618, PANEL);
  snprintf(buf, sizeof(buf), "Karta: %s", uid.c_str());
  lcd.drawString(buf, 225, 192);

  char tbuf[32] = "";
  snprintf(tbuf, sizeof(tbuf), "%s", nowString().c_str());
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(0xC618, PANEL);
  lcd.drawString(tbuf, 225, 212);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(color, PANEL);
  lcd.drawString(action, 225, 238);
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
  loadLogs();

  // RFID
  rfidHardwareInit();

  // Ekran
  lcd.init();
  lcd.setRotation(1);
  drawBackground();

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
    updateClock();
    rfidLoop();
  }

  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (!rfidOk) rfidHardwareInit();
    Serial.printf("Holat: RFID=%s | o'quvchi=%d | log=%d | ichida=%d | AP klient=%d | heap=%u\n",
                  rfidOk ? "tayyor" : "topilmadi", (int)students.size(),
                  (int)logs.size(), insideCount(), WiFi.softAPgetStationNum(),
                  (unsigned)ESP.getFreeHeap());
    if (!cardShowing && !rfidOk) drawCornerLabels();
  }

  delay(5);
}
