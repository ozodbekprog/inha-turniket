# INHA Turniket Admin (Android ilova)

Dashboard'ni Android ilova sifatida ko'rsatuvchi WebView ilova.

- Default server: `http://192.168.4.1` (ESP WiFi)
- Menyu → "Manzil (server)": manzilni o'zgartirish (masalan `http://10.0.2.2:8080` emulyatorda)
- Menyu → "Yangilash": sahifani qayta yuklash

## Qurish
```bash
export ANDROID_HOME=$HOME/Android/Sdk
export JAVA_HOME=$HOME/jdk17   # JDK 17 kerak
./build.sh
# natija: inha-turniket-admin.apk
```
Gradle'siz — to'g'ridan-to'g'ri aapt2 + javac + d8 + zipalign + apksigner.
