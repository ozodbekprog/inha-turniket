// TTPU TURNIKET — Google Apps Script (rasm + Kirgan/Chiqgan + Dashboard grafik)
// Bitta skript: kartani tekshiradi, rasm/ism/guruh qaytaradi, davomat yozadi,
// har urilishda KIRDI/CHIQDI almashinuvini yuritadi va statistikani yangilaydi.
//
// Varaqlar (avtomatik yaratiladi):
//   "Oquvchilar": UID | Ism | Familiya | Guruh | Holat | Rasm
//   "Davomat":    Vaqt | UID | Ism | Guruh | Chiqish | Holat
//   "Dashboard":  statistika + doiraviy grafik

const SPREADSHEET_ID = '1a9UTsT7iSbxMGcnA9OHZIH52XXnm9dzhbyOfL6QJjiU';

const USERS_SHEET = 'Oquvchilar';
const LOG_SHEET   = 'Davomat';
const DASH_SHEET  = 'Dashboard';

function doGet(e) {
  return handleRequest((e && e.parameter) || {});
}

function doPost(e) {
  var p = {};
  try { p = JSON.parse(e.postData.contents); } catch (err) { p = (e && e.parameter) || {}; }
  return handleRequest(p);
}

function handleRequest(p) {
  var uid = String(p.uid || '').toUpperCase().trim();
  var ss = SpreadsheetApp.openById(SPREADSHEET_ID);

  var us = ensureSheet(ss, USERS_SHEET, ['UID', 'Ism', 'Familiya', 'Guruh', 'Holat', 'Rasm']);
  var ls = ensureSheet(ss, LOG_SHEET, ['Vaqt', 'UID', 'Ism', 'Guruh', 'Chiqish', 'Holat']);

  var name = '', group = '', photo = '', holat = '';
  var known = false, allowed = false;

  if (uid !== '') {
    var data = us.getDataRange().getValues();
    var formulas = us.getRange(1, 6, data.length, 1).getFormulas();
    for (var i = 1; i < data.length; i++) {
      if (String(data[i][0] || '').toUpperCase().trim() === uid) {
        name  = String(data[i][1] || '') + ' ' + String(data[i][2] || '');
        group = String(data[i][3] || '');
        holat = String(data[i][4] || '').toLowerCase();
        allowed = (holat !== 'bloklangan' && holat !== 'blok');
        known = true;

        photo = getPhotoBase64(data[i][5], formulas[i][0]);

        // Rasm jadvalda ko'rinishi uchun IMAGE formulaga o'girish
        if (String(formulas[i][0]).indexOf('IMAGE(') < 0) {
          var mm = String(formulas[i][0] + ' ' + data[i][5]).match(/[-\w]{25,}/);
          if (mm) {
            us.getRange(i + 1, 6).setFormula(
              '=IMAGE("https://drive.google.com/thumbnail?id=' + mm[0] + '&sz=w400", 4, 120, 150)');
          }
        }
        break;
      }
    }
  }

  var now = new Date();
  var timeString = Utilities.formatDate(now, Session.getScriptTimeZone(), 'yyyy-MM-dd HH:mm:ss');
  var action = '';

  if (known && allowed) {
    // Kirgan/Chiqgan almashinuvi
    var dData = ls.getDataRange().getValues();
    var rowIndex = -1;
    for (var j = 1; j < dData.length; j++) {
      if (String(dData[j][1]).toUpperCase() === uid && String(dData[j][5]) === 'Kirgan') {
        rowIndex = j + 1;
        break;
      }
    }
    if (rowIndex != -1) {
      ls.getRange(rowIndex, 5).setValue(timeString);  // Chiqish vaqti
      ls.getRange(rowIndex, 6).setValue('Chiqgan');
      action = 'CHIQDI';
    } else {
      ls.appendRow([timeString, uid, name, group, '', 'Kirgan']);
      action = 'KIRDI';
    }
  } else {
    ls.appendRow([timeString, uid, known ? name : 'NOTANISH', group, '',
                  known ? 'BLOKLANGAN' : "RUXSAT YO'Q"]);
    action = known ? 'BLOKLANGAN' : "RUXSAT YO'Q";
  }

  updateStatisticsAndChart(ss);

  var result = {
    ok: true,
    uid: uid,
    name: name,
    group: group,
    allowed: allowed,
    photo: photo,
    action: action
  };
  return ContentService.createTextOutput(JSON.stringify(result))
    .setMimeType(ContentService.MimeType.JSON);
}

function ensureSheet(ss, name, headers) {
  var sh = ss.getSheetByName(name);
  if (!sh) {
    sh = ss.insertSheet(name);
    sh.appendRow(headers);
    sh.setFrozenRows(1);
  }
  return sh;
}

// Drive havolasi yoki IMAGE formulasidan rasmni base64 qilib qaytaradi
function getPhotoBase64(value, formula) {
  var combined = String(formula || '') + ' ' + String(value || '');
  var m = combined.match(/[-\w]{25,}/);
  if (!m) return '';
  var id = m[0];

  try {
    var url = 'https://drive.google.com/thumbnail?id=' + id + '&sz=w320';
    var resp = UrlFetchApp.fetch(url, { muteHttpExceptions: true });
    if (resp.getResponseCode() !== 200) return '';
    var bytes = resp.getBlob().getBytes();
    if (bytes.length > 80000) return '';
    return Utilities.base64Encode(bytes);
  } catch (err) {
    return '';
  }
}

// Statistika + doiraviy grafik (Dashboard varag'ida)
function updateStatisticsAndChart(ss) {
  var us = ss.getSheetByName(USERS_SHEET);
  var ls = ss.getSheetByName(LOG_SHEET);
  var dash = ensureSheet(ss, DASH_SHEET, ['Holati', 'Soni']);

  var total = Math.max(0, us.getLastRow() - 1);
  var d = ls.getDataRange().getValues();
  var present = 0;
  for (var i = 1; i < d.length; i++) {
    if (String(d[i][5]) === 'Kirgan') present++;
  }
  var absent = Math.max(0, total - present);

  dash.getRange('G1').setValue('Holati');
  dash.getRange('H1').setValue('Soni');
  dash.getRange('G2').setValue('Kirgan');
  dash.getRange('H2').setValue(present);
  dash.getRange('G3').setValue('Kirmagan / Chiqqan');
  dash.getRange('H3').setValue(absent);

  var charts = dash.getCharts();
  for (var c = 0; c < charts.length; c++) dash.removeChart(charts[c]);

  var chart = dash.newChart()
    .asPieChart()
    .setTitle('Bugungi talabalar davomati')
    .addRange(dash.getRange('G1:H3'))
    .setPosition(2, 10, 0, 0)
    .setOption('pieHole', 0.4)
    .setOption('colors', ['#27ae60', '#e74c3c'])
    .build();
  dash.insertChart(chart);
}
