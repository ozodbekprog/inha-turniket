#!/usr/bin/env bash
# INHA Turniket Admin — APK qurish skripti (Gradle'siz, to'g'ridan-to'g'ri SDK vositalari)
set -e

SDK="${ANDROID_HOME:-$HOME/Android/Sdk}"
BT="$SDK/build-tools/34.0.0"
AJAR="$SDK/platforms/android-34/android.jar"
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

rm -rf build gen
mkdir -p build gen

echo "[1/6] Resurslarni kompilyatsiya qilish..."
"$BT/aapt2" compile --dir res -o build/res.zip

echo "[2/6] Link (manifest + resurslar)..."
"$BT/aapt2" link -o build/base.apk -I "$AJAR" \
    --manifest AndroidManifest.xml -R build/res.zip \
    --java gen --min-sdk-version 24 --target-sdk-version 34 \
    --auto-add-overlay

echo "[3/6] Java kompilyatsiya..."
javac --release 8 -classpath "$AJAR" -d build/classes \
    gen/com/inha/turniket/R.java java/com/inha/turniket/MainActivity.java

echo "[4/6] DEX (d8)..."
mkdir -p build/dex
"$BT/d8" --release --min-api 24 --lib "$AJAR" \
    --output build/dex build/classes/com/inha/turniket/*.class

echo "[5/6] APK yig'ish + zipalign..."
python3 - <<'PY'
import zipfile, shutil, os
shutil.copy('build/base.apk', 'build/unsigned.apk')
z = zipfile.ZipFile('build/unsigned.apk', 'a', zipfile.ZIP_DEFLATED)
z.write('build/dex/classes.dex', 'classes.dex')
z.close()
PY
"$BT/zipalign" -p -f 4 build/unsigned.apk build/aligned.apk

echo "[6/6] Imzolash (debug kalit)..."
if [ ! -f debug.keystore ]; then
    keytool -genkeypair -keystore debug.keystore -alias androiddebugkey \
        -storepass android -keypass android -keyalg RSA -keysize 2048 \
        -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
fi
"$BT/apksigner" sign --ks debug.keystore --ks-pass pass:android \
    --key-pass pass:android --out inha-turniket-admin.apk build/aligned.apk

echo "TAYYOR: $DIR/inha-turniket-admin.apk"
ls -lh inha-turniket-admin.apk
