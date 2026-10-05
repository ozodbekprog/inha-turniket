// INHA University Turniket — Dashboard HTML (ESP ichida saqlanadi)
#pragma once

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="uz"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>INHA Turniket</title>
<style>
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;background:#0b1120;color:#e8eef7}
header{background:linear-gradient(90deg,#101a30,#0b1120);padding:14px 20px;display:flex;align-items:center;gap:10px;border-bottom:1px solid #22304a;position:sticky;top:0;z-index:5}
header h1{font-size:16px;margin:0;font-weight:700;letter-spacing:.3px}
header .dot{width:10px;height:10px;border-radius:50%;background:#2ecc71;box-shadow:0 0 10px #2ecc71}
header .time{margin-left:auto;color:#8fa3bf;font-size:13px}
main{padding:18px;max-width:1150px;margin:0 auto}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-bottom:16px}
.card{background:#121a2c;border:1px solid #22304a;border-radius:14px;padding:14px 16px}
.card .n{font-size:30px;font-weight:800;line-height:1}.card .t{color:#8fa3bf;font-size:12px;margin-top:6px}
.card.b .n{color:#60a5fa}.card.g .n{color:#2ecc71}.card.y .n{color:#f1c40f}.card.o .n{color:#f39c12}
.panel{background:#121a2c;border:1px solid #22304a;border-radius:14px;padding:14px;margin-bottom:16px}
.charts{display:grid;grid-template-columns:280px 1fr;gap:14px}
@media(max-width:760px){.charts{grid-template-columns:1fr}}
h2{font-size:12px;color:#8fa3bf;margin:0 0 10px;text-transform:uppercase;letter-spacing:.6px}
.legend{display:flex;gap:14px;font-size:12px;color:#8fa3bf;margin-top:8px;justify-content:center}
.legend i{display:inline-block;width:10px;height:10px;border-radius:3px;margin-right:5px}
.two{display:grid;grid-template-columns:1fr 1fr;gap:14px}
@media(max-width:760px){.two{grid-template-columns:1fr}}
table{width:100%;border-collapse:collapse;background:#121a2c;border-radius:12px;overflow:hidden}
th,td{padding:8px 10px;text-align:left;font-size:13px;border-bottom:1px solid #1b263b}
th{color:#8fa3bf;font-size:10px;text-transform:uppercase;letter-spacing:.4px}
tr:last-child td{border-bottom:none}
.badge{padding:2px 9px;border-radius:999px;font-size:11px;font-weight:700}
.kirdi{background:#123d26;color:#2ecc71}.chiqdi{background:#3d2a12;color:#f39c12}
.deny{background:#4a1a1a;color:#ff7675}
.students{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:12px}
.stu{background:#121a2c;border:1px solid #22304a;border-radius:12px;padding:12px;display:flex;gap:11px;align-items:center}
.stu img{width:56px;height:56px;border-radius:50%;object-fit:cover;border:2px solid #2b3c5c;background:#1b263b}
.stu .info{flex:1;min-width:0}.stu .info b{display:block;font-size:14px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.stu .info span{color:#8fa3bf;font-size:12px}
.stu .uid{color:#5c7099;font-size:10px}
.stu button{background:#3a1b1b;color:#ff7675;border:0;border-radius:8px;padding:7px 9px;cursor:pointer}
.stu .in{color:#2ecc71;font-size:11px;font-weight:600}
form.add{background:#121a2c;border:1px solid #22304a;border-radius:14px;padding:14px;display:grid;grid-template-columns:1fr 1fr;gap:10px}
form.add input[type=text]{background:#0b1120;border:1px solid #2b3c5c;color:#fff;padding:10px;border-radius:8px;font-size:14px}
form.add input[type=file]{color:#8fa3bf;font-size:13px;grid-column:1/-1}
form.add button{background:#1f6feb;color:#fff;border:0;padding:11px;border-radius:8px;font-size:14px;font-weight:700;grid-column:1/-1;cursor:pointer}
.footer{color:#5c7099;font-size:11px;margin-top:22px;text-align:center}
.muted{color:#5c7099;font-size:12px;padding:8px 2px}
.tabs{display:flex;gap:8px;margin-bottom:10px}
.tabs button{flex:1;background:#0b1120;border:1px solid #2b3c5c;color:#8fa3bf;padding:11px;border-radius:10px;font-size:13px;font-weight:700;cursor:pointer}
.tabs button.active{background:#1f6feb;border-color:#1f6feb;color:#fff}
.tabs button b{opacity:.85}
canvas{width:100%;height:auto;display:block}
.tablewrap{overflow-x:auto}
@media(max-width:600px){
  main{padding:12px}
  header h1{font-size:13px}
  header .time{font-size:11px}
  .card .n{font-size:24px}.card{padding:12px}
  form.add{grid-template-columns:1fr}
  th,td{padding:6px 8px;font-size:12px}
  .stu img{width:48px;height:48px}
  .panel{padding:10px}
}
</style></head><body>
<header><span class="dot"></span><h1>INHA UNIVERSITY — Turniket</h1><span class="time" id="clock">--:--:--</span></header>
<main>
<div class="cards">
  <div class="card b"><div class="n" id="s-total">0</div><div class="t">O'quvchilar</div></div>
  <div class="card g"><div class="n" id="s-inside">0</div><div class="t">Hozir ichida</div></div>
  <div class="card y"><div class="n" id="s-kirdi">0</div><div class="t">Bugun kirdi</div></div>
  <div class="card o"><div class="n" id="s-chiqdi">0</div><div class="t">Bugun chiqdi</div></div>
</div>

<div class="charts">
  <div class="panel">
    <h2>Ichida / Tashqarida</h2>
    <canvas id="donut" width="240" height="180"></canvas>
    <div class="legend"><span><i style="background:#2ecc71"></i>Ichida</span><span><i style="background:#334155"></i>Tashqarida</span></div>
  </div>
  <div class="panel">
    <h2>Bugungi soatlik harakat</h2>
    <canvas id="bars" width="760" height="190"></canvas>
    <div class="legend"><span><i style="background:#60a5fa"></i>Kirdi</span><span><i style="background:#f39c12"></i>Chiqdi</span></div>
  </div>
</div>

<h2>Hozir ichida</h2>
<div class="students" id="insideList"><div class="muted">Bo'sh</div></div>

<div class="panel" style="margin-top:18px">
  <div class="tabs">
    <button id="tabK" class="active" onclick="showTab('k')">Kirdi (bugun) <b id="cntK">0</b></button>
    <button id="tabC" onclick="showTab('c')">Chiqdi (bugun) <b id="cntC">0</b></button>
  </div>
  <div class="tablewrap" id="wrapK"><table><tbody id="kirdiTable"></tbody></table></div>
  <div class="tablewrap" id="wrapC" style="display:none"><table><tbody id="chiqdiTable"></tbody></table></div>
</div>

<h2 style="margin-top:22px">Yangi o'quvchi qo'shish</h2>
<form class="add" method="POST" action="/add" enctype="multipart/form-data">
  <input type="text" name="uid" placeholder="Karta UID (masalan: 00B9B41A)" required>
  <input type="text" name="sid" placeholder="Talaba ID (U2610266)">
  <input type="text" name="ism" placeholder="Ism" required>
  <input type="text" name="familiya" placeholder="Familiya" required>
  <input type="text" name="guruh" placeholder="Guruh (S00101)">
  <input type="file" name="photo" accept="image/jpeg,image/png">
  <button type="submit">Qo'shish</button>
</form>

<h2 style="margin-top:22px">Barcha o'quvchilar</h2>
<div class="students" id="students"></div>
<div class="footer">INHA University Turniket • 192.168.4.1 • har 3s yangilanadi</div>
</main>
<script>
function esc(s){return(s||'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function hhmm(ts){return (ts||'').length>=16?ts.substring(11,16):ts}
async function syncTime(){try{const d=new Date();await fetch('/api/time?ms='+d.getTime());document.getElementById('clock').textContent=d.toTimeString().substring(0,8);}catch(e){}}

function fitCanvas(c){const w=Math.floor(c.clientWidth||c.width);if(w&&c.width!==w){c.width=w;}}
function drawDonut(inside, outside){
  const c=document.getElementById('donut');fitCanvas(c);c.height=180;
  const x=c.getContext('2d');
  const W=c.width,H=c.height,cx=W/2,cy=H/2+6,R=Math.min(64,W/3.6);
  x.clearRect(0,0,W,H); x.lineWidth=24;
  const total=inside+outside;
  const a1=total? (inside/total)*Math.PI*2 : 0;
  x.strokeStyle='#334155'; x.beginPath(); x.arc(cx,cy,R,0,Math.PI*2); x.stroke();
  x.strokeStyle='#2ecc71'; x.beginPath(); x.arc(cx,cy,R,-Math.PI/2,-Math.PI/2+a1); x.stroke();
  x.fillStyle='#e8eef7'; x.font='700 22px system-ui'; x.textAlign='center';
  x.fillText(String(inside),cx,cy+2);
  x.fillStyle='#8fa3bf'; x.font='11px system-ui'; x.fillText('ichida',cx,cy+20);
}
function drawBars(hk,hc){
  const c=document.getElementById('bars');fitCanvas(c);c.height=190;
  const x=c.getContext('2d');
  const W=c.width,H=c.height,pad=26,bw=(W-pad*2)/24;
  x.clearRect(0,0,W,H);
  let mx=1; for(let i=0;i<24;i++){mx=Math.max(mx,hk[i],hc[i]);}
  x.strokeStyle='#1b263b'; x.beginPath(); x.moveTo(pad,H-18); x.lineTo(W-6,H-18); x.stroke();
  for(let i=0;i<24;i++){
    const bh1=(H-40)*(hk[i]/mx), bh2=(H-40)*(hc[i]/mx);
    const bx=pad+i*bw;
    x.fillStyle='#60a5fa'; x.fillRect(bx+1,H-18-bh1,bw*0.42,bh1);
    x.fillStyle='#f39c12'; x.fillRect(bx+bw*0.46,H-18-bh2,bw*0.42,bh2);
  }
  x.fillStyle='#5c7099'; x.font='10px system-ui'; x.textAlign='center';
  for(let i=0;i<24;i+=3){x.fillText(String(i).padStart(2,'0'),pad+i*bw+bw/2,H-6);}
}
function row(l){
  return `<tr><td style="width:64px">${esc(hhmm(l.ts))}</td><td>${esc(l.ism)} ${esc(l.familiya)}</td>
  <td style="color:#8fa3bf">${esc(l.sid)}</td><td><span class="badge ${l.holat==='Kirdi'?'kirdi':'chiqdi'}">${esc(l.holat)}</span></td></tr>`;
}
function showTab(t){
  const k=t==='k';
  document.getElementById('wrapK').style.display=k?'block':'none';
  document.getElementById('wrapC').style.display=k?'none':'block';
  document.getElementById('tabK').classList.toggle('active',k);
  document.getElementById('tabC').classList.toggle('active',!k);
}
async function load(){
  try{
    const r=await fetch('/api/data');const d=await r.json();
    document.getElementById('s-total').textContent=d.total;
    document.getElementById('s-inside').textContent=d.inside;
    document.getElementById('s-kirdi').textContent=d.todayKirdi;
    document.getElementById('s-chiqdi').textContent=d.todayChiqdi;
    document.getElementById('cntK').textContent=d.todayKirdi;
    document.getElementById('cntC').textContent=d.todayChiqdi;
    drawDonut(d.inside,d.total-d.inside);
    drawBars(d.hoursK,d.hoursC);
    document.getElementById('insideList').innerHTML=d.insideList.length?d.insideList.map(s=>`
      <div class="stu">${s.photo?`<img src="/photos/${esc(s.photo)}">`:''}
      <div class="info"><b>${esc(s.ism)} ${esc(s.familiya)}</b><span>${esc(s.sid)}</span>
      <div class="in">ichida • ${esc(hhmm(s.ts))}</div></div></div>`).join(''):'<div class="muted">Bo\'sh</div>';
    document.getElementById('kirdiTable').innerHTML=d.kirdiList.length?d.kirdiList.map(row).join(''):'<tr><td class="muted">Bo\'sh</td></tr>';
    document.getElementById('chiqdiTable').innerHTML=d.chiqdiList.length?d.chiqdiList.map(row).join(''):'<tr><td class="muted">Bo\'sh</td></tr>';
    document.getElementById('students').innerHTML=d.students.map(s=>`
      <div class="stu">${s.photo?`<img src="/photos/${esc(s.photo)}">`:''}
      <div class="info"><b>${esc(s.ism)} ${esc(s.familiya)}</b><span>${esc(s.sid)}</span>
      <div class="uid">${esc(s.uid)}</div></div>
      <button onclick="del('${esc(s.uid)}')">✕</button></div>`).join('');
  }catch(e){}
}
async function del(uid){if(!confirm('O\'chirilsinmi?'))return;await fetch('/api/del?uid='+encodeURIComponent(uid));load();}
syncTime();load();setInterval(load,3000);setInterval(syncTime,30000);
</script></body></html>)HTML";
