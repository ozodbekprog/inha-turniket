#!/usr/bin/env python3
# INHA TURNIKET — jonli server (kutubxonasiz, stdlib)
# - ESP qurilma: /api/event orqali hodisalarni yuboradi (GPRS)
# - Mobil app (Flutter/WebView): ESP bilan bir xil API (/api/data, /api/del, /add, /api/export)
# - Jonli sayt: http://localhost:8080 (dashboard)
# Ishga tushirish: python3 server.py

import json, os, re, sqlite3, threading, zlib, zipfile, io
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs, unquote

BASE = os.path.dirname(os.path.abspath(__file__))
DB = os.path.join(BASE, 'turniket.db')
PHOTOS = os.path.join(BASE, 'photos')
PORT = 8080

db_lock = threading.Lock()

def db():
    con = sqlite3.connect(DB)
    con.row_factory = sqlite3.Row
    return con

def init_db():
    con = db()
    con.executescript('''
    CREATE TABLE IF NOT EXISTS students(
      uid TEXT PRIMARY KEY, ism TEXT, familiya TEXT, guruh TEXT,
      holat TEXT DEFAULT 'faol', photo TEXT DEFAULT '');
    CREATE TABLE IF NOT EXISTS logs(
      id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT, uid TEXT, ism TEXT,
      guruh TEXT, holat TEXT);
    ''')
    try:
        con.execute("ALTER TABLE students ADD COLUMN sid TEXT DEFAULT ''")
    except sqlite3.OperationalError:
        pass
    con.execute('CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT)')
    con.execute("INSERT OR IGNORE INTO meta(key,value) VALUES('students_rev','1')")
    if con.execute('SELECT COUNT(*) c FROM students').fetchone()['c'] == 0:
        con.execute("INSERT INTO students(uid,ism,familiya,guruh,holat,photo,sid) VALUES(?,?,?,?,?,?,?)",
                    ('00B9B41A', 'Bekmirza', 'Jumaboyev', 'S00101 (1-kurs)', 'faol', '00B9B41A.jpg', ''))
    con.commit()
    con.close()

def today():
    return datetime.now().strftime('%Y-%m-%d')

def shrink_photo(path):
    """Rasmni kichraytiradi (ekran uchun yetarli: 480px, JPEG q75)."""
    try:
        from PIL import Image
        im = Image.open(path)
        im = im.convert('RGB')
        im.thumbnail((360, 360))
        im.save(path, 'JPEG', quality=70)
    except Exception:
        pass

def bump_rev(con):
    con.execute("UPDATE meta SET value=CAST(CAST(value AS INTEGER)+1 AS TEXT) WHERE key='students_rev'")

def handle_card(uid):
    uid = (uid or '').strip().upper()
    with db_lock:
        con = db()
        s = con.execute('SELECT * FROM students WHERE UPPER(uid)=?', (uid,)).fetchone()
        now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        if not s:
            con.execute('INSERT INTO logs(ts,uid,ism,guruh,holat) VALUES(?,?,?,?,?)',
                        (now, uid, 'NOTANISH', '', "Ruxsat yo'q"))
            con.commit(); con.close()
            return {"ok": 1, "known": 0, "allowed": 0, "name": "", "group": "",
                    "photo": "", "action": "RUXSAT YO'Q"}
        name = (s['ism'] + ' ' + s['familiya']).strip()
        allowed = 1 if str(s['holat']).lower() not in ('bloklangan', 'blok') else 0
        if not allowed:
            con.execute('INSERT INTO logs(ts,uid,ism,guruh,holat) VALUES(?,?,?,?,?)',
                        (now, uid, name, s['guruh'], 'Bloklangan'))
            con.commit(); con.close()
            return {"ok": 1, "known": 1, "allowed": 0, "name": name,
                    "group": s['guruh'], "photo": s['photo'], "action": "BLOKLANGAN"}
        last = con.execute(
            "SELECT holat FROM logs WHERE uid=? AND ts LIKE ? ORDER BY id DESC LIMIT 1",
            (uid, today() + '%')).fetchone()
        holat = 'Chiqdi' if (last and last['holat'] == 'Kirdi') else 'Kirdi'
        con.execute('INSERT INTO logs(ts,uid,ism,guruh,holat) VALUES(?,?,?,?,?)',
                    (now, uid, name, s['guruh'], holat))
        con.commit(); con.close()
        return {"ok": 1, "known": 1, "allowed": 1, "name": name,
                "group": s['guruh'], "photo": s['photo'], "action": holat.upper()}

# ---------- ESP bilan bir xil /api/data formati (mobil app uchun) ----------

def esp_data():
    d0 = today()
    with db_lock:
        con = db()
        students = [dict(r) for r in con.execute('SELECT * FROM students ORDER BY ism')]
        logs = [dict(r) for r in con.execute('SELECT * FROM logs ORDER BY id')]
        con.close()

    # oxirgi holat har bir uid uchun
    last_state, last_ts = {}, {}
    for l in logs:
        if l['holat'] == 'Kirdi':
            last_state[l['uid']] = 'Kirdi'; last_ts[l['uid']] = l['ts']
        elif l['holat'] == 'Chiqdi':
            last_state[l['uid']] = 'Chiqdi'; last_ts[l['uid']] = l['ts']

    today_k = today_c = 0
    hk = [0] * 24
    hc = [0] * 24
    for l in logs:
        if not l['ts'].startswith(d0):
            continue
        if l['holat'] == 'Kirdi':
            today_k += 1
            try:
                h = int(l['ts'][11:13]); hk[h % 24] += 1
            except Exception:
                pass
        elif l['holat'] == 'Chiqdi':
            today_c += 1
            try:
                h = int(l['ts'][11:13]); hc[h % 24] += 1
            except Exception:
                pass

    kirdi_list, chiqdi_list = [], []
    for s in students:
        st = last_state.get(s['uid'])
        if st == 'Kirdi':
            kirdi_list.append({"ts": last_ts.get(s['uid'], ""), "ism": s['ism'],
                               "familiya": s['familiya'], "sid": s.get('sid', ''),
                               "photo": s['photo'] or '', "holat": "Kirdi"})
        elif st == 'Chiqdi':
            chiqdi_list.append({"ts": last_ts.get(s['uid'], ""), "ism": s['ism'],
                                "familiya": s['familiya'], "sid": s.get('sid', ''),
                                "photo": s['photo'] or '', "holat": "Chiqdi"})

    return {
        "total": len(students),
        "inside": len(kirdi_list),
        "todayKirdi": today_k,
        "todayChiqdi": today_c,
        "hoursK": hk,
        "hoursC": hc,
        "kirdiList": kirdi_list,
        "chiqdiList": chiqdi_list,
        "students": [{"uid": s['uid'], "ism": s['ism'], "familiya": s['familiya'],
                      "sid": s.get('sid', ''), "photo": s['photo'] or ''} for s in students],
    }

# ---------- Server dashboard uchun (eski format) ----------

def board_data():
    with db_lock:
        con = db()
        students = [dict(r) for r in con.execute('SELECT * FROM students ORDER BY ism')]
        logs = [dict(r) for r in con.execute('SELECT * FROM logs ORDER BY id DESC LIMIT 80')]
        rows = con.execute("SELECT uid,holat FROM logs WHERE ts LIKE ? ORDER BY id",
                           (today() + '%',)).fetchall()
        con.close()
    kirdi, inside = set(), set()
    denied_today = 0
    for r in rows:
        if r['holat'] == 'Kirdi':
            kirdi.add(r['uid']); inside.add(r['uid'])
        elif r['holat'] == 'Chiqdi':
            inside.discard(r['uid'])
        elif r['holat'] in ("Ruxsat yo'q", 'Bloklangan'):
            denied_today += 1
    return {"students": students, "logs": logs,
            "stats": {"total": len(students), "today": len(kirdi),
                      "inside": len(inside), "denied": denied_today}}

# ---------- XLSX eksport (minimal, zipfile bilan) ----------

def _xml_esc(s):
    return (str(s or '').replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
            .replace('"', '&quot;').replace("'", '&apos;'))

def _xml_row(cells):
    r = '<row>'
    for c in cells:
        r += '<c t="inlineStr"><is><t xml:space="preserve">' + _xml_esc(c) + '</t></is></c>'
    r += '</row>'
    return r

def build_xlsx():
    with db_lock:
        con = db()
        logs = [dict(r) for r in con.execute('SELECT * FROM logs ORDER BY id DESC LIMIT 1000')]
        students = [dict(r) for r in con.execute('SELECT * FROM students ORDER BY ism')]
        con.close()

    s1 = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    s1 += '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'
    s1 += _xml_row(["Vaqt", "UID", "Ism", "Guruh", "Holat"])
    for l in logs:
        s1 += _xml_row([l['ts'], l['uid'], l['ism'], l['guruh'], l['holat']])
    s1 += '</sheetData></worksheet>'

    s2 = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    s2 += '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'
    s2 += _xml_row(["UID", "Ism", "Familiya", "Talaba ID", "Guruh", "Holat"])
    for s in students:
        s2 += _xml_row([s['uid'], s['ism'], s['familiya'], s.get('sid', ''), s['guruh'], s['holat']])
    s2 += '</sheetData></worksheet>'

    ct = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
          '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="xml" ContentType="application/xml"/>'
          '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
          '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
          '<Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
          '</Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
            '</Relationships>')
    wb = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
          '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
          'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>'
          '<sheet name="Davomat" sheetId="1" r:id="rId1"/>'
          '<sheet name="Oquvchilar" sheetId="2" r:id="rId2"/>'
          '</sheets></workbook>')
    wbrels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
              '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
              '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>'
              '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>'
              '</Relationships>')

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', ct)
        z.writestr('_rels/.rels', rels)
        z.writestr('xl/workbook.xml', wb)
        z.writestr('xl/_rels/workbook.xml.rels', wbrels)
        z.writestr('xl/worksheets/sheet1.xml', s1)
        z.writestr('xl/worksheets/sheet2.xml', s2)
    return buf.getvalue()

# ---------- Multipart parser (stdlib) ----------

def parse_multipart(body, ctype):
    m = re.search(r'boundary=([^;]+)', ctype or '')
    if not m:
        return {}, {}
    boundary = m.group(1).strip().strip('"').encode()
    fields, files = {}, {}
    for part in body.split(b'--' + boundary):
        part = part.strip(b'\r\n')
        if not part or part == b'--':
            continue
        if b'\r\n\r\n' not in part:
            continue
        head, data = part.split(b'\r\n\r\n', 1)
        if data.endswith(b'\r\n'):
            data = data[:-2]
        hd = head.decode('utf-8', 'replace')
        nm = re.search(r'name="([^"]+)"', hd)
        fn = re.search(r'filename="([^"]*)"', hd)
        if not nm:
            continue
        if fn and fn.group(1):
            files[nm.group(1)] = (fn.group(1), data)
        else:
            fields[nm.group(1)] = data.decode('utf-8', 'replace')
    return fields, files

def clean_field(s):
    s = (s or '').replace(',', ' ').replace('\n', ' ').replace('\r', ' ').strip()
    return s

# ---------- Dashboard HTML ----------

DASH_FILE = os.path.join(BASE, 'dashboard.html')

def load_page():
    try:
        with open(DASH_FILE, encoding='utf-8') as f:
            return f.read()
    except OSError:
        return '<h1>dashboard.html topilmadi</h1>'


class H(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send(self, code, body, ctype='application/json; charset=utf-8', extra=None):
        if isinstance(body, (dict, list)):
            body = json.dumps(body, ensure_ascii=False)
        if isinstance(body, str):
            body = body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Access-Control-Allow-Origin', '*')
        if extra:
            for k, v in extra.items():
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path == '/':
            return self._send(200, load_page(), 'text/html; charset=utf-8')
        if u.path == '/api/data':
            return self._send(200, esp_data())
        if u.path == '/api/board':
            return self._send(200, board_data())
        if u.path == '/api/time':
            return self._send(200, {"ok": 1})
        if u.path == '/api/now':
            return self._send(200, {"ms": int(datetime.now().timestamp() * 1000)})
        if u.path == '/api/card':
            uid = (q.get('uid') or [''])[0]
            return self._send(200, handle_card(uid))
        if u.path == '/api/event':
            ts = (q.get('ts') or [''])[0]
            uid = (q.get('uid') or [''])[0]
            ism = (q.get('ism') or [''])[0]
            guruh = (q.get('guruh') or [''])[0]
            holat = (q.get('holat') or [''])[0]
            if uid and holat:
                with db_lock:
                    con = db()
                    con.execute('INSERT INTO logs(ts,uid,ism,guruh,holat) VALUES(?,?,?,?,?)',
                                (ts, uid, ism, guruh, holat))
                    con.commit(); con.close()
            return self._send(200, {"ok": 1})
        if u.path == '/api/student':
            uid = (q.get('uid') or [''])[0].strip().upper()
            ism = (q.get('ism') or [''])[0]
            familiya = (q.get('familiya') or [''])[0]
            sid = (q.get('sid') or [''])[0]
            guruh = (q.get('guruh') or [''])[0]
            holat = (q.get('holat') or [''])[0] or 'faol'
            if uid:
                with db_lock:
                    con = db()
                    row = con.execute('SELECT photo FROM students WHERE uid=?', (uid,)).fetchone()
                    photo = row['photo'] if row else ''
                    con.execute('INSERT OR REPLACE INTO students(uid,ism,familiya,guruh,holat,photo,sid) '
                                'VALUES(?,?,?,?,?,?,?)',
                                (uid, ism, familiya, guruh, holat, photo, sid))
                    con.commit(); con.close()
            return self._send(200, {"ok": 1})
        if u.path == '/api/students/tsv':
            try:
                client_rev = int((q.get('rev') or ['0'])[0])
            except ValueError:
                client_rev = 0
            def clean(x):
                return str(x or '').replace('|', ' ').replace('\n', ' ').replace('\r', ' ')
            with db_lock:
                con = db()
                row = con.execute("SELECT value FROM meta WHERE key='students_rev'").fetchone()
                rev = int(row['value']) if row else 1
                out = "REV %d\n" % rev
                if client_rev != rev:
                    for s in con.execute('SELECT uid,ism,familiya,sid,guruh,holat,photo FROM students ORDER BY uid'):
                        out += "|".join([clean(s['uid']), clean(s['ism']), clean(s['familiya']),
                                         clean(s['sid']), clean(s['guruh']), clean(s['holat']),
                                         clean(os.path.basename(s['photo'] or ''))]) + "\n"
                con.close()
            return self._send(200, out, 'text/plain; charset=utf-8')
        if u.path == '/api/unknown':
            with db_lock:
                con = db()
                rows = con.execute(
                    "SELECT uid, COUNT(*) cnt, MAX(ts) last_ts FROM logs "
                    "WHERE holat=? GROUP BY uid ORDER BY last_ts DESC LIMIT 50",
                    ("Ruxsat yo'q",)).fetchall()
                known = set(r['uid'] for r in con.execute('SELECT uid FROM students').fetchall())
                con.close()
            out = [{"uid": r['uid'], "count": r['cnt'], "ts": r['last_ts']}
                   for r in rows if r['uid'] not in known]
            return self._send(200, out)
        if u.path == '/api/week':
            from datetime import timedelta
            days = []
            with db_lock:
                con = db()
                for i in range(6, -1, -1):
                    day = (datetime.now() - timedelta(days=i)).strftime('%Y-%m-%d')
                    k = con.execute("SELECT COUNT(*) c FROM logs WHERE ts LIKE ? AND holat='Kirdi'",
                                    (day + '%',)).fetchone()['c']
                    c = con.execute("SELECT COUNT(*) c FROM logs WHERE ts LIKE ? AND holat='Chiqdi'",
                                    (day + '%',)).fetchone()['c']
                    days.append({"date": day, "kirdi": k, "chiqdi": c})
                con.close()
            return self._send(200, days)
        if u.path == '/api/del':
            uid = (q.get('uid') or [''])[0]
            with db_lock:
                con = db()
                row = con.execute('SELECT photo FROM students WHERE uid=?', (uid,)).fetchone()
                if row and row['photo']:
                    p = os.path.join(PHOTOS, os.path.basename(row['photo']))
                    if os.path.isfile(p):
                        try:
                            os.remove(p)
                        except OSError:
                            pass
                con.execute('DELETE FROM students WHERE uid=?', (uid,))
                bump_rev(con)
                con.commit(); con.close()
            return self._send(200, {"ok": 1})
        if u.path == '/api/reset':
            with db_lock:
                con = db(); con.execute('DELETE FROM logs'); con.commit(); con.close()
            return self._send(200, {"ok": 1})
        if u.path == '/api/export':
            data = build_xlsx()
            return self._send(200, data,
                              'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
                              {'Content-Disposition': 'attachment; filename="inha-turniket.xlsx"'})
        if u.path.startswith('/photos/'):
            name = os.path.basename(unquote(u.path))
            if not name or '..' in name:
                return self._send(404, {"error": "not found"})
            path = os.path.join(PHOTOS, name)
            if os.path.isfile(path):
                with open(path, 'rb') as f:
                    return self._send(200, f.read(), 'image/jpeg')
            return self._send(404, {"error": "not found"})
        return self._send(404, {"error": "not found"})

    def do_POST(self):
        u = urlparse(self.path)
        if u.path == '/api/import':
            length = int(self.headers.get('Content-Length', 0) or 0)
            body = self.rfile.read(length) if length else b''
            fields, files = parse_multipart(body, self.headers.get('Content-Type', ''))
            if 'file' in files:
                text = files['file'][1].decode('utf-8-sig', 'replace')
            elif fields.get('csv'):
                text = fields['csv']
            else:
                text = body.decode('utf-8-sig', 'replace')
            count = 0
            with db_lock:
                con = db()
                for line in text.splitlines():
                    line = line.strip().rstrip('\r')
                    if not line or line.lower().startswith('uid,'):
                        continue
                    p = [x.strip() for x in line.split(',')]
                    if len(p) < 3 or not p[0]:
                        continue
                    uid = p[0].upper()
                    ism = p[1] if len(p) > 1 else ''
                    familiya = p[2] if len(p) > 2 else ''
                    sid = guruh = holat = photo = ''
                    if len(p) >= 7:
                        sid, guruh, holat, photo = p[3], p[4], p[5], p[6]
                    elif len(p) == 6:
                        guruh, holat, photo = p[3], p[4], p[5]
                    elif len(p) == 5:
                        guruh, holat = p[3], p[4]
                    elif len(p) == 4:
                        guruh = p[3]
                    con.execute('INSERT OR REPLACE INTO students(uid,ism,familiya,guruh,holat,photo,sid) '
                                'VALUES(?,?,?,?,?,?,?)',
                                (uid, ism, familiya, guruh, holat or 'faol', photo, sid))
                    count += 1
                if count:
                    bump_rev(con)
                con.commit(); con.close()
            return self._send(200, {"ok": 1, "count": count})
        if u.path != '/add':
            return self._send(404, {"error": "not found"})
        length = int(self.headers.get('Content-Length', 0) or 0)
        body = self.rfile.read(length) if length else b''
        fields, files = parse_multipart(body, self.headers.get('Content-Type', ''))
        uid = clean_field(fields.get('uid', '')).upper()
        ism = clean_field(fields.get('ism', ''))
        familiya = clean_field(fields.get('familiya', ''))
        sid = clean_field(fields.get('sid', ''))
        guruh = clean_field(fields.get('guruh', ''))
        holat = clean_field(fields.get('holat', '')).lower()
        if 'blok' in holat:
            holat = 'bloklangan'
        elif holat and 'faol' not in holat:
            holat = 'faol'
        if not uid:
            return self._send(400, {"error": "uid kerak"})

        photo_name = ''
        if 'photo' in files and files['photo'][1]:
            base = re.sub(r'[^A-Za-z0-9]', '', uid)
            gen = format(int(datetime.now().timestamp()) % 16, 'x')
            stem = base[:6] + '_' + gen       # FFat 8.3 uchun qisqa nom (8 belgi)
            photo_name = stem + '.jpg'
            os.makedirs(PHOTOS, exist_ok=True)
            with open(os.path.join(PHOTOS, photo_name), 'wb') as f:
                f.write(files['photo'][1])
            shrink_photo(os.path.join(PHOTOS, photo_name))

        with db_lock:
            con = db()
            row = con.execute('SELECT * FROM students WHERE uid=?', (uid,)).fetchone()
            if row:
                # eski rasmni o'chirish (nomi o'zgargan bo'lsa)
                if photo_name and row['photo'] and row['photo'] != photo_name:
                    old = os.path.join(PHOTOS, os.path.basename(row['photo']))
                    if os.path.isfile(old):
                        try:
                            os.remove(old)
                        except OSError:
                            pass
                con.execute('UPDATE students SET ism=?,familiya=?,guruh=?,sid=?,photo=?,holat=? WHERE uid=?',
                            (ism or row['ism'], familiya or row['familiya'],
                             guruh or row['guruh'], sid or row['sid'],
                             photo_name or row['photo'], holat or row['holat'], uid))
            elif ism:
                con.execute('INSERT INTO students(uid,ism,familiya,guruh,holat,photo,sid) VALUES(?,?,?,?,?,?,?)',
                            (uid, ism, familiya, guruh, holat or 'faol', photo_name, sid))
            bump_rev(con)
            con.commit(); con.close()

        self.send_response(303)
        self.send_header('Location', '/')
        self.send_header('Content-Length', '0')
        self.end_headers()

if __name__ == '__main__':
    init_db()
    # Mavjud katta rasmlarni bir marta kichraytirish
    try:
        for fn in os.listdir(PHOTOS):
            if fn.lower().endswith(('.jpg', '.jpeg', '.png')):
                p = os.path.join(PHOTOS, fn)
                if os.path.getsize(p) > 35000:
                    shrink_photo(p)
                    print('Kichraytirildi:', fn, os.path.getsize(p), 'bayt')
    except OSError:
        pass
    srv = ThreadingHTTPServer(('0.0.0.0', PORT), H)
    print(f'INHA Turniket jonli server http://localhost:{PORT} da ishlayapti')
    srv.serve_forever()
