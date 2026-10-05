#!/usr/bin/env python3
# TTPU TURNIKET — lokal dashboard server (kutubxonasiz, stdlib)
# Ishga tushirish: python3 server.py
# Dashboard: http://localhost:8080
# ESP uchun:  http://<PC-IP>:8080/api/card?uid=XXXX

import json, os, sqlite3, threading
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

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
    if con.execute('SELECT COUNT(*) c FROM students').fetchone()['c'] == 0:
        con.execute("INSERT INTO students(uid,ism,familiya,guruh,holat,photo) VALUES(?,?,?,?,?,?)",
                    ('00B9B41A', 'Bekmirza', 'Jumaboyev', 'S00101 (1-kurs)', 'faol', '/photos/bekmirza.jpg'))
    con.commit()
    con.close()

def today():
    return datetime.now().strftime('%Y-%m-%d')

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

def get_data():
    con = db()
    students = [dict(r) for r in con.execute('SELECT * FROM students ORDER BY ism')]
    logs = [dict(r) for r in con.execute('SELECT * FROM logs ORDER BY id DESC LIMIT 80')]
    rows = con.execute("SELECT uid,holat FROM logs WHERE ts LIKE ? ORDER BY id", (today() + '%',)).fetchall()
    kirdi, inside = set(), set()
    denied_today = 0
    for r in rows:
        if r['holat'] == 'Kirdi':
            kirdi.add(r['uid']); inside.add(r['uid'])
        elif r['holat'] == 'Chiqdi':
            inside.discard(r['uid'])
        elif r['holat'] in ('Ruxsat yo\'q', 'Bloklangan'):
            denied_today += 1
    con.close()
    return {"students": students, "logs": logs,
            "stats": {"total": len(students), "today": len(kirdi),
                      "inside": len(inside), "denied": denied_today}}

PAGE = '''<!doctype html><html lang="uz"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TTPU Turniket — Dashboard</title>
<style>
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;background:#0e1420;color:#e8eef7}
header{background:#131c2e;padding:16px 24px;display:flex;align-items:center;gap:12px;border-bottom:1px solid #22304a}
header h1{font-size:18px;margin:0;font-weight:600}
header .dot{width:10px;height:10px;border-radius:50%;background:#27ae60;box-shadow:0 0 8px #27ae60}
main{padding:20px 24px;max-width:1200px;margin:0 auto}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px;margin-bottom:20px}
.card{background:#131c2e;border:1px solid #22304a;border-radius:12px;padding:16px}
.card .n{font-size:32px;font-weight:700}.card .t{color:#8fa3bf;font-size:13px;margin-top:4px}
.card.g .n{color:#2ecc71}.card.y .n{color:#f1c40f}.card.r .n{color:#e74c3c}.card.b .n{color:#3498db}
h2{font-size:15px;color:#8fa3bf;margin:24px 0 10px;font-weight:600;text-transform:uppercase;letter-spacing:.5px}
table{width:100%;border-collapse:collapse;background:#131c2e;border-radius:12px;overflow:hidden}
th,td{padding:10px 14px;text-align:left;font-size:14px;border-bottom:1px solid #1b263b}
th{color:#8fa3bf;font-weight:600;font-size:12px;text-transform:uppercase}
tr:last-child td{border-bottom:none}
.badge{padding:3px 10px;border-radius:999px;font-size:12px;font-weight:600}
.kirdi{background:#123d26;color:#2ecc71}.chiqdi{background:#3d2a12;color:#f39c12}
.deny{background:#4a1a1a;color:#ff7675}
.students{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:14px}
.stu{background:#131c2e;border:1px solid #22304a;border-radius:12px;padding:14px;display:flex;gap:12px;align-items:center}
.stu img{width:64px;height:64px;border-radius:50%;object-fit:cover;border:2px solid #2b3c5c;background:#1b263b}
.stu .info b{display:block}.stu .info span{color:#8fa3bf;font-size:13px}
.stu .uid{color:#5c7099;font-size:11px;margin-top:2px}
.footer{color:#5c7099;font-size:12px;margin-top:24px;text-align:center}
</style></head><body>
<header><span class="dot"></span><h1>TTPU TURNIKET — Jonli dashboard</h1></header>
<main>
<div class="cards">
  <div class="card b"><div class="n" id="s-total">0</div><div class="t">Jami o'quvchilar</div></div>
  <div class="card g"><div class="n" id="s-today">0</div><div class="t">Bugun kirgan</div></div>
  <div class="card y"><div class="n" id="s-inside">0</div><div class="t">Hozir ichida</div></div>
  <div class="card r"><div class="n" id="s-denied">0</div><div class="t">Ruxsat yo'q (bugun)</div></div>
</div>
<h2>O'quvchilar</h2>
<div class="students" id="students"></div>
<h2>Davomat (oxirgi 80)</h2>
<table><thead><tr><th>Vaqt</th><th>UID</th><th>Ism</th><th>Guruh</th><th>Holat</th></tr></thead>
<tbody id="logs"></tbody></table>
<div class="footer">Har 3 sekundda yangilanadi • http://localhost:8080</div>
</main>
<script>
function cls(h){h=h.toLowerCase();if(h.includes('kirdi'))return'kirdi';if(h.includes('chiqdi'))return'chiqdi';return'deny'}
function esc(s){return(s||'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function load(){
  const r=await fetch('/api/data');const d=await r.json();
  document.getElementById('s-total').textContent=d.stats.total;
  document.getElementById('s-today').textContent=d.stats.today;
  document.getElementById('s-inside').textContent=d.stats.inside;
  document.getElementById('s-denied').textContent=d.stats.denied;
  document.getElementById('students').innerHTML=d.students.map(s=>`
    <div class="stu">${s.photo?`<img src="${esc(s.photo)}" onerror="this.style.visibility='hidden'">`:''}
    <div class="info"><b>${esc(s.ism)} ${esc(s.familiya)}</b><span>${esc(s.guruh)}</span>
    <div class="uid">${esc(s.uid)} • ${esc(s.holat)}</div></div></div>`).join('');
  document.getElementById('logs').innerHTML=d.logs.map(l=>`
    <tr><td>${esc(l.ts)}</td><td>${esc(l.uid)}</td><td>${esc(l.ism)}</td>
    <td>${esc(l.guruh)}</td><td><span class="badge ${cls(l.holat)}">${esc(l.holat)}</span></td></tr>`).join('');
}
load();setInterval(load,3000);
</script></body></html>'''

class H(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send(self, code, body, ctype='application/json; charset=utf-8'):
        if isinstance(body, (dict, list)):
            body = json.dumps(body, ensure_ascii=False)
        if isinstance(body, str):
            body = body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path == '/':
            return self._send(200, PAGE, 'text/html; charset=utf-8')
        if u.path == '/api/data':
            return self._send(200, get_data())
        if u.path == '/api/card':
            uid = (q.get('uid') or [''])[0]
            return self._send(200, handle_card(uid))
        if u.path == '/api/reset':
            with db_lock:
                con = db(); con.execute('DELETE FROM logs'); con.commit(); con.close()
            return self._send(200, {"ok": 1})
        if u.path.startswith('/photos/'):
            name = os.path.basename(u.path)
            path = os.path.join(PHOTOS, name)
            if os.path.isfile(path):
                with open(path, 'rb') as f:
                    return self._send(200, f.read(), 'image/jpeg')
            return self._send(404, {"error": "not found"})
        return self._send(404, {"error": "not found"})

if __name__ == '__main__':
    init_db()
    srv = ThreadingHTTPServer(('0.0.0.0', PORT), H)
    print(f'TTPU Turniket server http://localhost:{PORT} da ishlayapti')
    srv.serve_forever()
