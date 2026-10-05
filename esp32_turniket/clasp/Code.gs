// TTPU TURNIKET — Google Apps Script (Bekmirzo akkaunti uchun, o'zi o'rnatiladi)
// Birinchi chaqiruvda o'zi "TTPU Turniket" jadvalini yaratadi va rasmni ko'chiradi.
//
// Varaqlar: Oquvchilar | Davomat | Dashboard

const USERS_SHEET = 'Oquvchilar';
const LOG_SHEET   = 'Davomat';
const DASH_SHEET  = 'Dashboard';
const PROP_KEY    = 'SPREADSHEET_ID';

// Boshlang'ich o'quvchi (rasm Ozodbek akkauntidagi ochiq havoladan ko'chiriladi)
const SEED_PHOTO_URL = 'https://drive.google.com/thumbnail?id=1X7M8YcU0tWH5mC70xjUbDAMB5f5oo19B&sz=w400';
const SEED_UID   = '00B9B41A';
const SEED_NAME  = 'Bekmirza';
const SEED_FAM   = 'Jumaboyev';
const SEED_GROUP = 'S00101 (1-kurs)';

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
  var ss = getSpreadsheet_();

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
    var dData = ls.getDataRange().getValues();
    var rowIndex = -1;
    for (var j = 1; j < dData.length; j++) {
      if (String(dData[j][1]).toUpperCase() === uid && String(dData[j][5]) === 'Kirgan') {
        rowIndex = j + 1;
        break;
      }
    }
    if (rowIndex != -1) {
      ls.getRange(rowIndex, 5).setValue(timeString);
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
    ok: true, uid: uid, name: name, group: group,
    allowed: allowed, photo: photo, action: action,
    sheet: ss.getUrl()
  };
  return ContentService.createTextOutput(JSON.stringify(result))
    .setMimeType(ContentService.MimeType.JSON);
}

// ---------- O'ZI O'RNATILISH ----------

function getSpreadsheet_() {
  var props = PropertiesService.getScriptProperties();
  var id = props.getProperty(PROP_KEY);
  var ss = null;
  if (id) {
    try { ss = SpreadsheetApp.openById(id); } catch (e) { ss = null; }
  }
  if (!ss) {
    ss = SpreadsheetApp.create('TTPU Turniket');
    props.setProperty(PROP_KEY, ss.getId());
    seedSpreadsheet_(ss);
  }
  return ss;
}

function seedSpreadsheet_(ss) {
  var us = ensureSheet(ss, USERS_SHEET, ['UID', 'Ism', 'Familiya', 'Guruh', 'Holat', 'Rasm']);
  ensureSheet(ss, LOG_SHEET, ['Vaqt', 'UID', 'Ism', 'Guruh', 'Chiqish', 'Holat']);
  ensureSheet(ss, DASH_SHEET, ['Holati', 'Soni']);

  if (us.getLastRow() < 2) {
    var photoId = copySeedPhoto_();
    var cell = photoId
      ? '=IMAGE("https://drive.google.com/thumbnail?id=' + photoId + '&sz=w400", 4, 120, 150)'
      : '';
    us.appendRow([SEED_UID, SEED_NAME, SEED_FAM, SEED_GROUP, 'faol', cell]);
  }
}

function copySeedPhoto_() {
  try {
    var files = DriveApp.getFilesByName('Bekmirza_Jumaboyev.jpg');
    if (files.hasNext()) {
      var f = files.next();
      f.setSharing(DriveApp.Access.ANYONE_WITH_LINK, DriveApp.Permission.VIEW);
      return f.getId();
    }
    var resp = UrlFetchApp.fetch(SEED_PHOTO_URL, { muteHttpExceptions: true });
    if (resp.getResponseCode() !== 200) return '';
    var blob = resp.getBlob().setName('Bekmirza_Jumaboyev.jpg');
    var file = DriveApp.createFile(blob);
    file.setSharing(DriveApp.Access.ANYONE_WITH_LINK, DriveApp.Permission.VIEW);
    return file.getId();
  } catch (e) {
    return '';
  }
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

// Bir marta qo'lda ishga tushiriladigan sozlash (ruxsatlar + jadval yaratish)
function setup() {
  var ss = getSpreadsheet_();
  Logger.log('Jadval: ' + ss.getUrl());
}
