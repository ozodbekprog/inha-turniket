import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:http/http.dart' as http;
import 'package:image_picker/image_picker.dart';
import 'package:local_auth/local_auth.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:url_launcher/url_launcher.dart';

// ================= ADMINLAR =================
const Map<String, String> ADMINS = {
  'tursunmuratov': 'prorektor123',
  'boburxojimirzayev': 'boburboss',
  'bekmirzo': 'bekmirzotutor',
};

final LocalAuthentication auth = LocalAuthentication();

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final prefs = await SharedPreferences.getInstance();
  final user = prefs.getString('user');
  final fast = prefs.getBool('fastlogin') ?? false;
  runApp(TurniketApp(
    loggedUser: fast ? null : user,
    autoBio: fast && user != null,
  ));
}

class TurniketApp extends StatelessWidget {
  const TurniketApp({super.key, this.loggedUser, this.autoBio = false});
  final String? loggedUser;
  final bool autoBio;

  @override
  Widget build(BuildContext context) {
    final base = ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      colorScheme: ColorScheme.fromSeed(
        seedColor: const Color(0xFF1F6FEB),
        brightness: Brightness.dark,
      ),
      scaffoldBackgroundColor: const Color(0xFF0B1120),
      cardTheme: CardThemeData(
        color: const Color(0xFF121A2C),
        elevation: 0,
        shape: RoundedRectangleBorder(
          side: const BorderSide(color: Color(0xFF22304A)),
          borderRadius: BorderRadius.circular(14),
        ),
      ),
    );
    return MaterialApp(
      title: 'INHA Turniket Admin',
      debugShowCheckedModeBanner: false,
      theme: base,
      home: loggedUser == null
          ? LoginPage(autoBio: autoBio)
          : DashboardPage(user: loggedUser!),
    );
  }
}

// ================= LOGIN =================

class LoginPage extends StatefulWidget {
  const LoginPage({super.key, this.autoBio = false});
  final bool autoBio;

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final _loginCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  bool _obscure = true;
  String? _error;
  bool _loading = false;
  bool _fastLogin = false;
  String? _lastUser;

  @override
  void initState() {
    super.initState();
    _loadFast();
  }

  Future<void> _loadFast() async {
    final prefs = await SharedPreferences.getInstance();
    final f = prefs.getBool('fastlogin') ?? false;
    final u = prefs.getString('lastuser');
    if (!mounted) return;
    setState(() {
      _fastLogin = f;
      _lastUser = u;
    });
    if (widget.autoBio && f && u != null && u.isNotEmpty) {
      await Future.delayed(const Duration(milliseconds: 400));
      _tryBiometric(auto: true);
    }
  }

  Future<void> _tryBiometric({bool auto = false}) async {
    final user = _lastUser ?? '';
    if (user.isEmpty) return;
    try {
      final ok = await auth.authenticate(
        localizedReason: 'INHA Turniket: $user sifatida kirish uchun tasdiqlang',
        biometricOnly: false,
        sensitiveTransaction: true,
        persistAcrossBackgrounding: true,
      );
      if (ok) {
        final prefs = await SharedPreferences.getInstance();
        await prefs.setString('user', user);
        if (!mounted) return;
        Navigator.of(context).pushReplacement(
          MaterialPageRoute(builder: (_) => DashboardPage(user: user)),
        );
      } else if (!auto) {
        setState(() => _error = 'Tasdiqlanmadi — parol bilan kiring');
      }
    } on PlatformException catch (e) {
      if (!auto) {
        setState(() => _error = 'Biometrik xato: ${e.message ?? e.code}');
      }
    }
  }

  @override
  void dispose() {
    _loginCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    final login = _loginCtrl.text.trim().toLowerCase();
    final pass = _passCtrl.text;
    setState(() {
      _loading = true;
      _error = null;
    });
    await Future.delayed(const Duration(milliseconds: 400));
    if (ADMINS.containsKey(login) && ADMINS[login] == pass) {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('user', login);
      final already = prefs.getBool('fastlogin') ?? false;
      if (!already && mounted) {
        final en = await showDialog<bool>(
          context: context,
          builder: (ctx) => AlertDialog(
            title: const Text('Face ID / barmoq izi'),
            content: const Text(
                'Keyingi kirishlarda Face ID yoki barmoq izi bilan tez kirishni yoqasizmi?'),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(ctx, false),
                child: const Text('Yo\'q'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(ctx, true),
                child: const Text('Ha, yoqilsin'),
              ),
            ],
          ),
        );
        if (en == true) {
          await prefs.setBool('fastlogin', true);
          await prefs.setString('lastuser', login);
        }
      }
      if (!mounted) return;
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(builder: (_) => DashboardPage(user: login)),
      );
    } else {
      setState(() {
        _error = 'Login yoki parol xato';
        _loading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: SingleChildScrollView(
          padding: const EdgeInsets.symmetric(horizontal: 28, vertical: 24),
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 420),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                // Yuqorida INHA logotipi
                Container(
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    boxShadow: [
                      BoxShadow(
                        color: const Color(0xFF1F6FEB).withOpacity(0.35),
                        blurRadius: 30,
                        spreadRadius: 2,
                      ),
                    ],
                  ),
                  child: Image.asset('assets/logo.png', width: 130, height: 130),
                ),
                const SizedBox(height: 18),
                const Text(
                  'INHA UNIVERSITY',
                  style: TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 1.2,
                  ),
                ),
                const SizedBox(height: 4),
                const Text(
                  'Turniket boshqaruv tizimi',
                  style: TextStyle(color: Color(0xFF8FA3BF), fontSize: 13),
                ),
                const SizedBox(height: 28),
                TextField(
                  controller: _loginCtrl,
                  textInputAction: TextInputAction.next,
                  decoration: InputDecoration(
                    labelText: 'Login',
                    prefixIcon: const Icon(Icons.person_outline),
                    filled: true,
                    fillColor: const Color(0xFF121A2C),
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(12),
                      borderSide: const BorderSide(color: Color(0xFF2B3C5C)),
                    ),
                  ),
                ),
                const SizedBox(height: 14),
                TextField(
                  controller: _passCtrl,
                  obscureText: _obscure,
                  onSubmitted: (_) => _submit(),
                  decoration: InputDecoration(
                    labelText: 'Parol',
                    prefixIcon: const Icon(Icons.lock_outline),
                    suffixIcon: IconButton(
                      icon: Icon(_obscure
                          ? Icons.visibility_off_outlined
                          : Icons.visibility_outlined),
                      onPressed: () => setState(() => _obscure = !_obscure),
                    ),
                    filled: true,
                    fillColor: const Color(0xFF121A2C),
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(12),
                      borderSide: const BorderSide(color: Color(0xFF2B3C5C)),
                    ),
                  ),
                ),
                if (_error != null) ...[
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      const Icon(Icons.error_outline,
                          color: Color(0xFFFF7675), size: 18),
                      const SizedBox(width: 6),
                      Text(_error!,
                          style: const TextStyle(
                              color: Color(0xFFFF7675), fontSize: 13)),
                    ],
                  ),
                ],
                const SizedBox(height: 20),
                SizedBox(
                  width: double.infinity,
                  height: 50,
                  child: FilledButton(
                    onPressed: _loading ? null : _submit,
                    style: FilledButton.styleFrom(
                      backgroundColor: const Color(0xFF1F6FEB),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                    ),
                    child: _loading
                        ? const SizedBox(
                            width: 22,
                            height: 22,
                            child: CircularProgressIndicator(
                                strokeWidth: 2, color: Colors.white),
                          )
                        : const Text('Kirish',
                            style: TextStyle(
                                fontSize: 16, fontWeight: FontWeight.w700)),
                  ),
                ),
                if (_fastLogin && _lastUser != null) ...[
                  const SizedBox(height: 12),
                  SizedBox(
                    width: double.infinity,
                    height: 48,
                    child: OutlinedButton.icon(
                      onPressed: () => _tryBiometric(),
                      icon: const Icon(Icons.face, color: Color(0xFF60A5FA)),
                      label: Text(
                        'Face ID bilan kirish ($_lastUser)',
                        style: const TextStyle(color: Color(0xFF60A5FA)),
                      ),
                    ),
                  ),
                ],
                const SizedBox(height: 14),
                const Text(
                  'Faqat administratorlar uchun',
                  style: TextStyle(color: Color(0xFF5C7099), fontSize: 11),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

// ================= API =================

class Api {
  static String base = 'http://192.168.4.1';

  static Future<Map<String, dynamic>> getData() async {
    final client = HttpClient()..connectionTimeout = const Duration(seconds: 4);
    try {
      final req = await client
          .getUrl(Uri.parse('$base/api/data'))
          .timeout(const Duration(seconds: 5));
      final res = await req.close().timeout(const Duration(seconds: 6));
      final body = await res.transform(utf8.decoder).join();
      if (res.statusCode != 200) throw Exception('HTTP ${res.statusCode}');
      return jsonDecode(body) as Map<String, dynamic>;
    } finally {
      client.close(force: true);
    }
  }

  static Future<void> timeSync() async {
    try {
      final client = HttpClient()
        ..connectionTimeout = const Duration(seconds: 3);
      final ms = DateTime.now().millisecondsSinceEpoch;
      final req = await client
          .getUrl(Uri.parse('$base/api/time?ms=$ms'))
          .timeout(const Duration(seconds: 3));
      await (await req.close().timeout(const Duration(seconds: 3))).drain();
      client.close(force: true);
    } catch (_) {}
  }

  static Future<void> delete(String uid) async {
    final client = HttpClient()..connectionTimeout = const Duration(seconds: 4);
    try {
      final req = await client
          .getUrl(Uri.parse('$base/api/del?uid=${Uri.encodeComponent(uid)}'))
          .timeout(const Duration(seconds: 5));
      await (await req.close().timeout(const Duration(seconds: 5))).drain();
    } finally {
      client.close(force: true);
    }
  }

  // Yangi o'quvchini to'g'ridan-to'g'ri ESP bazasiga yuklash (rasm bilan)
  static Future<bool> addStudent({
    required String uid,
    required String ism,
    required String familiya,
    String sid = '',
    String guruh = '',
    String? photoPath,
  }) async {
    try {
      final req = http.MultipartRequest('POST', Uri.parse('$base/add'));
      req.fields['uid'] = uid;
      req.fields['ism'] = ism;
      req.fields['familiya'] = familiya;
      req.fields['sid'] = sid;
      req.fields['guruh'] = guruh;
      if (photoPath != null && photoPath.isNotEmpty) {
        req.files
            .add(await http.MultipartFile.fromPath('photo', photoPath));
      }
      final streamed = await req.send().timeout(const Duration(seconds: 30));
      final res = await http.Response.fromStream(streamed);
      return res.statusCode == 200 || res.statusCode == 303;
    } catch (_) {
      return false;
    }
  }

  static String photoUrl(String photo) => '$base/photos/$photo';
}

// ================= DASHBOARD =================

class DashboardPage extends StatefulWidget {
  const DashboardPage({super.key, required this.user});
  final String user;

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  Map<String, dynamic>? data;
  String? error;
  bool loading = true;
  int tab = 0; // 0 = Kirdi, 1 = Chiqdi
  bool _fastLoginEnabled = false;
  Timer? timer;

  @override
  void initState() {
    super.initState();
    _init();
  }

  @override
  void dispose() {
    timer?.cancel();
    super.dispose();
  }

  Future<void> _init() async {
    final prefs = await SharedPreferences.getInstance();
    Api.base = prefs.getString('server') ?? 'http://192.168.4.1';
    _fastLoginEnabled = prefs.getBool('fastlogin') ?? false;
    await _load();
    timer =
        Timer.periodic(const Duration(seconds: 5), (_) => _load(silent: true));
  }

  Future<void> _load({bool silent = false}) async {
    if (!silent) {
      setState(() {
        loading = true;
        error = null;
      });
    }
    try {
      final d = await Api.getData();
      Api.timeSync();
      if (mounted) {
        setState(() {
          data = d;
          error = null;
          loading = false;
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          if (!silent || data == null) {
            error = e.toString();
            loading = false;
          }
        });
      }
    }
  }

  Future<void> _openProfile() async {
    await showModalBottomSheet(
      context: context,
      backgroundColor: const Color(0xFF121A2C),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) => Padding(
        padding: const EdgeInsets.all(22),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Image.asset('assets/logo.png', width: 72, height: 72),
            const SizedBox(height: 10),
            Text(
              widget.user,
              style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 2),
            const Text('Administrator',
                style: TextStyle(color: Color(0xFF8FA3BF), fontSize: 13)),
            const SizedBox(height: 10),
            StatefulBuilder(
              builder: (sctx, setSheet) => SwitchListTile(
                contentPadding: EdgeInsets.zero,
                secondary:
                    const Icon(Icons.face, color: Color(0xFF8FA3BF)),
                title: const Text('Face ID bilan kirish',
                    style: TextStyle(fontSize: 14)),
                value: _fastLoginEnabled,
                onChanged: (v) async {
                  final prefs = await SharedPreferences.getInstance();
                  await prefs.setBool('fastlogin', v);
                  if (v) {
                    await prefs.setString('lastuser', widget.user);
                  } else {
                    await prefs.remove('lastuser');
                  }
                  setSheet(() {});
                  if (mounted) setState(() => _fastLoginEnabled = v);
                },
              ),
            ),
            const SizedBox(height: 8),
            SizedBox(
              width: double.infinity,
              child: FilledButton.icon(
                style: FilledButton.styleFrom(
                  backgroundColor: const Color(0xFF7F1D1D),
                  padding: const EdgeInsets.symmetric(vertical: 14),
                ),
                onPressed: () async {
                  final prefs = await SharedPreferences.getInstance();
                  await prefs.remove('user');
                  if (ctx.mounted) Navigator.pop(ctx);
                  if (mounted) {
                    Navigator.of(context).pushAndRemoveUntil(
                      MaterialPageRoute(builder: (_) => const LoginPage()),
                      (r) => false,
                    );
                  }
                },
                icon: const Icon(Icons.logout),
                label: const Text('Chiqish'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _openSettings() async {
    final ctrl = TextEditingController(text: Api.base);
    final saved = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Server (ESP) manzili'),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.url,
          decoration: const InputDecoration(
            hintText: 'http://192.168.4.1',
            helperText: 'ESP32 WiFi: INHA-TURNIKET',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Bekor'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, ctrl.text.trim()),
            child: const Text('Saqlash'),
          ),
        ],
      ),
    );
    if (saved != null && saved.isNotEmpty) {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('server', saved);
      Api.base = saved;
      _load();
    }
  }

  Future<void> _openAddStudent() async {
    final ok = await Navigator.of(context).push<bool>(
      MaterialPageRoute(builder: (_) => const AddStudentPage()),
    );
    if (ok == true) _load();
  }

  Future<void> _openExcel() async {
    final uri = Uri.parse('${Api.base}/api/export');
    if (!await launchUrl(uri, mode: LaunchMode.externalApplication)) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Brauzerni ochib bo\'lmadi')),
        );
      }
    }
  }

  Future<void> _delete(String uid, String name) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('O\'chirilsinmi?'),
        content: Text(name),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Yo\'q'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: Colors.red.shade800),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('O\'chirish'),
          ),
        ],
      ),
    );
    if (ok == true) {
      await Api.delete(uid);
      _load();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: const Color(0xFF101A30),
        title: const Text('INHA Turniket Admin',
            style: TextStyle(fontSize: 17, fontWeight: FontWeight.w700)),
        actions: [
          IconButton(
            tooltip: 'Yangi o\'quvchi',
            onPressed: _openAddStudent,
            icon: const Icon(Icons.person_add_alt),
          ),
          IconButton(
            tooltip: 'Excel yuklab olish',
            onPressed: data != null ? _openExcel : null,
            icon: const Icon(Icons.download),
          ),
          IconButton(
            tooltip: 'Yangilash',
            onPressed: () => _load(),
            icon: const Icon(Icons.refresh),
          ),
          IconButton(
            tooltip: 'Server manzili',
            onPressed: _openSettings,
            icon: const Icon(Icons.settings),
          ),
          IconButton(
            tooltip: 'Profil',
            onPressed: _openProfile,
            icon: const Icon(Icons.account_circle),
          ),
        ],
      ),
      body: _buildBody(),
    );
  }

  Widget _buildBody() {
    if (loading) {
      return const Center(child: CircularProgressIndicator());
    }
    if (error != null && data == null) {
      return _ConnectionHelp(
        error: error!,
        server: Api.base,
        onRetry: _load,
        onSettings: _openSettings,
      );
    }
    final d = data!;
    final students = (d['students'] as List).cast<Map<String, dynamic>>();
    final kirdi = (d['kirdiList'] as List).cast<Map<String, dynamic>>();
    final chiqdi = (d['chiqdiList'] as List).cast<Map<String, dynamic>>();
    final list = tab == 0 ? kirdi : chiqdi;

    return RefreshIndicator(
      onRefresh: _load,
      child: ListView(
        padding: const EdgeInsets.all(12),
        children: [
          if (error != null)
            Container(
              margin: const EdgeInsets.only(bottom: 10),
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: const Color(0xFF3D2A12),
                borderRadius: BorderRadius.circular(10),
              ),
              child: const Text(
                'Aloqa uzildi — qayta ulanmoqda...',
                style: TextStyle(color: Color(0xFFF39C12), fontSize: 12),
              ),
            ),
          _statsGrid(d),
          const SizedBox(height: 12),
          _charts(d),
          const SizedBox(height: 12),
          _tabsCard(list),
          const SizedBox(height: 12),
          const Text('BARCHA O\'QUVCHILAR',
              style: TextStyle(
                  color: Color(0xFF8FA3BF),
                  fontSize: 12,
                  fontWeight: FontWeight.w700,
                  letterSpacing: 0.5)),
          const SizedBox(height: 8),
          ...students.map((s) => _studentTile(s)),
          const SizedBox(height: 30),
        ],
      ),
    );
  }

  Widget _statsGrid(Map<String, dynamic> d) {
    final items = [
      ('${d['total'] ?? 0}', 'O\'quvchilar', const Color(0xFF60A5FA)),
      ('${d['inside'] ?? 0}', 'Hozir ichida', const Color(0xFF2ECC71)),
      ('${d['todayKirdi'] ?? 0}', 'Bugun kirdi', const Color(0xFFF1C40F)),
      ('${d['todayChiqdi'] ?? 0}', 'Bugun chiqdi', const Color(0xFFF39C12)),
    ];
    return GridView.count(
      crossAxisCount: 2,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      mainAxisSpacing: 10,
      crossAxisSpacing: 10,
      childAspectRatio: 2.1,
      children: items
          .map((it) => Card(
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Text(it.$1,
                          style: TextStyle(
                              fontSize: 26,
                              fontWeight: FontWeight.w800,
                              color: it.$3)),
                      const SizedBox(height: 4),
                      Text(it.$2,
                          style: const TextStyle(
                              fontSize: 12, color: Color(0xFF8FA3BF))),
                    ],
                  ),
                ),
              ))
          .toList(),
    );
  }

  Widget _charts(Map<String, dynamic> d) {
    final total = (d['total'] ?? 0) as int;
    final inside = (d['inside'] ?? 0) as int;
    final hk = (d['hoursK'] as List).cast<num>().map((e) => e.toInt()).toList();
    final hc = (d['hoursC'] as List).cast<num>().map((e) => e.toInt()).toList();
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('ICHIDA / TASHQARIDA',
                style: TextStyle(
                    color: Color(0xFF8FA3BF),
                    fontSize: 11,
                    fontWeight: FontWeight.w700,
                    letterSpacing: 0.5)),
            const SizedBox(height: 8),
            Row(
              children: [
                SizedBox(
                  width: 130,
                  height: 110,
                  child: CustomPaint(
                    painter: DonutPainter(inside: inside, total: total),
                  ),
                ),
                Expanded(
                  child: SizedBox(
                    height: 110,
                    child: CustomPaint(
                      painter: BarsPainter(hk: hk, hc: hc),
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 6),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: const [
                _Legend(color: Color(0xFF2ECC71), text: 'Ichida'),
                SizedBox(width: 14),
                _Legend(color: Color(0xFF60A5FA), text: 'Kirdi'),
                SizedBox(width: 14),
                _Legend(color: Color(0xFFF39C12), text: 'Chiqdi'),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _tabsCard(List<Map<String, dynamic>> list) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(10),
        child: Column(
          children: [
            Row(
              children: [
                Expanded(child: _tabButton(0, 'Kirdi')),
                const SizedBox(width: 8),
                Expanded(child: _tabButton(1, 'Chiqdi')),
              ],
            ),
            const SizedBox(height: 6),
            if (list.isEmpty)
              const Padding(
                padding: EdgeInsets.all(14),
                child: Text('Bo\'sh',
                    style: TextStyle(color: Color(0xFF5C7099), fontSize: 12)),
              )
            else
              ...list.map((l) => _logTile(l)),
          ],
        ),
      ),
    );
  }

  Widget _tabButton(int idx, String label) {
    final active = tab == idx;
    return InkWell(
      borderRadius: BorderRadius.circular(10),
      onTap: () => setState(() => tab = idx),
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 12),
        decoration: BoxDecoration(
          color: active ? const Color(0xFF1F6FEB) : const Color(0xFF0B1120),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
              color:
                  active ? const Color(0xFF1F6FEB) : const Color(0xFF2B3C5C)),
        ),
        child: Center(
          child: Text(
            label,
            style: TextStyle(
              color: active ? Colors.white : const Color(0xFF8FA3BF),
              fontWeight: FontWeight.w700,
              fontSize: 13,
            ),
          ),
        ),
      ),
    );
  }

  Widget _logTile(Map<String, dynamic> l) {
    final photo = (l['photo'] ?? '') as String;
    return ListTile(
      dense: true,
      contentPadding: EdgeInsets.zero,
      leading: _avatar(photo, 20),
      title: Text('${l['ism']} ${l['familiya']}',
          style: const TextStyle(fontSize: 14)),
      subtitle: Text('${l['sid']}  •  ${_hhmm(l['ts'] ?? '')}',
          style: const TextStyle(fontSize: 11, color: Color(0xFF8FA3BF))),
      trailing: Container(
        padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 3),
        decoration: BoxDecoration(
          color: l['holat'] == 'Kirdi'
              ? const Color(0xFF123D26)
              : const Color(0xFF3D2A12),
          borderRadius: BorderRadius.circular(999),
        ),
        child: Text(
          '${l['holat']}',
          style: TextStyle(
            fontSize: 11,
            fontWeight: FontWeight.w700,
            color: l['holat'] == 'Kirdi'
                ? const Color(0xFF2ECC71)
                : const Color(0xFFF39C12),
          ),
        ),
      ),
    );
  }

  Widget _studentTile(Map<String, dynamic> s) {
    final photo = (s['photo'] ?? '') as String;
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: _avatar(photo, 24),
        title: Text('${s['ism']} ${s['familiya']}',
            style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w600)),
        subtitle: Text('${s['sid']}\n${s['uid']}',
            style: const TextStyle(fontSize: 11, color: Color(0xFF8FA3BF))),
        isThreeLine: true,
        trailing: IconButton(
          icon: const Icon(Icons.close, color: Color(0xFFFF7675)),
          onPressed: () =>
              _delete('${s['uid']}', '${s['ism']} ${s['familiya']}'),
        ),
      ),
    );
  }

  Widget _avatar(String photo, double r) {
    if (photo.isEmpty) {
      return CircleAvatar(
        radius: r,
        backgroundColor: const Color(0xFF1B263B),
        child: Icon(Icons.person, size: r, color: const Color(0xFF5C7099)),
      );
    }
    return CircleAvatar(
      radius: r,
      backgroundColor: const Color(0xFF1B263B),
      backgroundImage: NetworkImage(Api.photoUrl(photo)),
      onBackgroundImageError: (_, __) {},
    );
  }

  String _hhmm(String ts) {
    if (ts.length >= 16) return ts.substring(11, 16);
    return ts;
  }
}

class _Legend extends StatelessWidget {
  const _Legend({required this.color, required this.text});
  final Color color;
  final String text;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 10,
          height: 10,
          decoration: BoxDecoration(
            color: color,
            borderRadius: BorderRadius.circular(3),
          ),
        ),
        const SizedBox(width: 5),
        Text(text,
            style: const TextStyle(fontSize: 11, color: Color(0xFF8FA3BF))),
      ],
    );
  }
}

class DonutPainter extends CustomPainter {
  DonutPainter({required this.inside, required this.total});
  final int inside;
  final int total;

  @override
  void paint(Canvas canvas, Size size) {
    final c = Offset(size.width / 2, size.height / 2);
    final r = math.min(size.width, size.height) / 2 - 14;
    final bg = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 18
      ..color = const Color(0xFF334155);
    final fg = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 18
      ..strokeCap = StrokeCap.round
      ..color = const Color(0xFF2ECC71);
    canvas.drawCircle(c, r, bg);
    if (total > 0) {
      final a = (inside / total) * math.pi * 2;
      canvas.drawArc(
          Rect.fromCircle(center: c, radius: r), -math.pi / 2, a, false, fg);
    }
    final tp = TextPainter(
      text: TextSpan(
        text: '$inside',
        style: const TextStyle(
            color: Colors.white, fontSize: 20, fontWeight: FontWeight.w800),
      ),
      textDirection: TextDirection.ltr,
    )..layout();
    tp.paint(canvas, c - Offset(tp.width / 2, tp.height / 2 + 8));
    final tp2 = TextPainter(
      text: const TextSpan(
        text: 'ichida',
        style: TextStyle(color: Color(0xFF8FA3BF), fontSize: 10),
      ),
      textDirection: TextDirection.ltr,
    )..layout();
    tp2.paint(canvas, c - Offset(tp2.width / 2, -tp2.height / 2 + 4));
  }

  @override
  bool shouldRepaint(covariant DonutPainter old) =>
      old.inside != inside || old.total != total;
}

class BarsPainter extends CustomPainter {
  BarsPainter({required this.hk, required this.hc});
  final List<int> hk;
  final List<int> hc;

  @override
  void paint(Canvas canvas, Size size) {
    final w = size.width, h = size.height;
    final pad = 6.0;
    final baseY = h - 14;
    var mx = 1;
    for (var i = 0; i < 24; i++) {
      final k = i < hk.length ? hk[i] : 0;
      final c = i < hc.length ? hc[i] : 0;
      mx = math.max(mx, math.max(k, c));
    }
    final paintK = Paint()..color = const Color(0xFF60A5FA);
    final paintC = Paint()..color = const Color(0xFFF39C12);
    final bw = (w - pad * 2) / 24;
    for (var i = 0; i < 24; i++) {
      final k = i < hk.length ? hk[i] : 0;
      final c = i < hc.length ? hc[i] : 0;
      final x = pad + i * bw;
      final h1 = (baseY - 6) * k / mx;
      final h2 = (baseY - 6) * c / mx;
      canvas.drawRect(Rect.fromLTWH(x + 1, baseY - h1, bw * 0.42, h1), paintK);
      canvas.drawRect(
          Rect.fromLTWH(x + bw * 0.46, baseY - h2, bw * 0.42, h2), paintC);
    }
    final line = Paint()
      ..color = const Color(0xFF1B263B)
      ..strokeWidth = 1;
    canvas.drawLine(Offset(pad, baseY), Offset(w - pad, baseY), line);
    for (var i = 0; i < 24; i += 6) {
      final tp = TextPainter(
        text: TextSpan(
          text: '${i.toString().padLeft(2, '0')}',
          style: const TextStyle(color: Color(0xFF5C7099), fontSize: 9),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(
          canvas, Offset(pad + i * bw + bw / 2 - tp.width / 2, baseY + 2));
    }
  }

  @override
  bool shouldRepaint(covariant BarsPainter old) => true;
}

class _ConnectionHelp extends StatelessWidget {
  const _ConnectionHelp({
    required this.error,
    required this.server,
    required this.onRetry,
    required this.onSettings,
  });
  final String error;
  final String server;
  final Future<void> Function({bool silent}) onRetry;
  final VoidCallback onSettings;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.wifi_off, size: 64, color: Color(0xFF5C7099)),
            const SizedBox(height: 14),
            const Text('ESP32 ga ulanmadi',
                style: TextStyle(fontSize: 20, fontWeight: FontWeight.w800)),
            const SizedBox(height: 8),
            const Text(
              'Telefonni ESP32 WiFi tarmog\'iga ulang:\n\n'
              '📶 INHA-TURNIKET\n🔑 turniket2026\n\n'
              'So\'ng "Qayta urinish" ni bosing.',
              textAlign: TextAlign.center,
              style: TextStyle(color: Color(0xFF8FA3BF), fontSize: 13),
            ),
            const SizedBox(height: 6),
            Text('Server: $server',
                style:
                    const TextStyle(color: Color(0xFF5C7099), fontSize: 11)),
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: () => onRetry(),
              icon: const Icon(Icons.refresh),
              label: const Text('Qayta urinish'),
            ),
            TextButton(
              onPressed: onSettings,
              child: const Text('Server manzilini o\'zgartirish'),
            ),
          ],
        ),
      ),
    );
  }
}

// ================= YANGI O'QUVCHI QO'SHISH =================

class AddStudentPage extends StatefulWidget {
  const AddStudentPage({super.key});

  @override
  State<AddStudentPage> createState() => _AddStudentPageState();
}

class _AddStudentPageState extends State<AddStudentPage> {
  final _uid = TextEditingController();
  final _ism = TextEditingController();
  final _familiya = TextEditingController();
  final _sid = TextEditingController();
  final _guruh = TextEditingController();
  final _picker = ImagePicker();
  XFile? _photo;
  bool _saving = false;

  @override
  void dispose() {
    _uid.dispose();
    _ism.dispose();
    _familiya.dispose();
    _sid.dispose();
    _guruh.dispose();
    super.dispose();
  }

  Future<void> _pick(ImageSource src) async {
    try {
      final x = await _picker.pickImage(
        source: src,
        maxWidth: 600,
        maxHeight: 600,
        imageQuality: 80,
      );
      if (x != null) setState(() => _photo = x);
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Rasm olishda xato: $e')),
        );
      }
    }
  }

  Future<void> _save() async {
    final uid = _uid.text.trim().toUpperCase();
    if (uid.isEmpty ||
        _ism.text.trim().isEmpty ||
        _familiya.text.trim().isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('UID, Ism va Familiya majburiy')),
      );
      return;
    }
    setState(() => _saving = true);
    final ok = await Api.addStudent(
      uid: uid,
      ism: _ism.text.trim(),
      familiya: _familiya.text.trim(),
      sid: _sid.text.trim(),
      guruh: _guruh.text.trim(),
      photoPath: _photo?.path,
    );
    if (!mounted) return;
    setState(() => _saving = false);
    if (ok) {
      Navigator.pop(context, true);
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
            content: Text('Saqlanmadi — ESP32 WiFi ulanishini tekshiring')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: const Color(0xFF101A30),
        title: const Text('Yangi o\'quvchi qo\'shish',
            style: TextStyle(fontSize: 17, fontWeight: FontWeight.w700)),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Center(
            child: GestureDetector(
              onTap: () => _pick(ImageSource.camera),
              child: Container(
                width: 132,
                height: 132,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: const Color(0xFF121A2C),
                  border: Border.all(
                      color: const Color(0xFF2B3C5C), width: 2),
                  image: _photo != null
                      ? DecorationImage(
                          image: FileImage(File(_photo!.path)),
                          fit: BoxFit.cover,
                        )
                      : null,
                ),
                child: _photo == null
                    ? const Icon(Icons.add_a_photo,
                        size: 42, color: Color(0xFF5C7099))
                    : null,
              ),
            ),
          ),
          const SizedBox(height: 8),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              TextButton.icon(
                onPressed: () => _pick(ImageSource.camera),
                icon: const Icon(Icons.camera_alt),
                label: const Text('Kamera'),
              ),
              const SizedBox(width: 10),
              TextButton.icon(
                onPressed: () => _pick(ImageSource.gallery),
                icon: const Icon(Icons.photo_library),
                label: const Text('Galereya'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          _field(_uid, 'Karta UID (masalan: 00B9B41A)'),
          _field(_ism, 'Ism'),
          _field(_familiya, 'Familiya'),
          _field(_sid, 'Talaba ID (U2610266)'),
          _field(_guruh, 'Guruh (S00101)'),
          const SizedBox(height: 16),
          SizedBox(
            height: 52,
            child: FilledButton.icon(
              onPressed: _saving ? null : _save,
              icon: _saving
                  ? const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(strokeWidth: 2))
                  : const Icon(Icons.save),
              label: Text(
                _saving ? 'Saqlanmoqda...' : 'Bazaga saqlash',
                style: const TextStyle(
                    fontSize: 15, fontWeight: FontWeight.w700),
              ),
            ),
          ),
          const SizedBox(height: 10),
          const Center(
            child: Text(
              'Rasm + ma\'lumotlar to\'g\'ridan-to\'g\'ri\nESP32 bazasiga saqlanadi',
              textAlign: TextAlign.center,
              style: TextStyle(color: Color(0xFF5C7099), fontSize: 11),
            ),
          ),
        ],
      ),
    );
  }

  Widget _field(TextEditingController c, String label) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: TextField(
        controller: c,
        decoration: InputDecoration(
          labelText: label,
          filled: true,
          fillColor: const Color(0xFF121A2C),
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(12),
            borderSide: const BorderSide(color: Color(0xFF2B3C5C)),
          ),
        ),
      ),
    );
  }
}
