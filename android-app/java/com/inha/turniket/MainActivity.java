package com.inha.turniket;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.view.Menu;
import android.view.MenuItem;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.Toast;

public class MainActivity extends Activity {
    private WebView web;
    private SharedPreferences prefs;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        prefs = getSharedPreferences("cfg", MODE_PRIVATE);

        web = new WebView(this);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setLoadWithOverviewMode(true);
        s.setUseWideViewPort(true);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);
        web.setWebViewClient(new WebViewClient());
        setContentView(web);

        String url = null;
        Intent it = getIntent();
        if (it != null && it.getStringExtra("url") != null) {
            url = it.getStringExtra("url");
        }
        if (url == null || url.length() == 0) {
            url = prefs.getString("url", "http://192.168.4.1");
        }
        web.loadUrl(url);
    }

    @Override
    public void onBackPressed() {
        if (web != null && web.canGoBack()) {
            web.goBack();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu m) {
        m.add(0, 1, 0, "Yangilash");
        m.add(0, 2, 0, "Manzil (server)");
        m.add(0, 3, 0, "Tashqi brauzerda");
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == 1) {
            web.reload();
            return true;
        }
        if (id == 2) {
            final EditText et = new EditText(this);
            et.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
            et.setText(prefs.getString("url", "http://192.168.4.1"));
            new AlertDialog.Builder(this)
                .setTitle("Server manzili")
                .setView(et)
                .setPositiveButton("Saqlash", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface d, int w) {
                        String u = et.getText().toString().trim();
                        if (u.length() > 0) {
                            prefs.edit().putString("url", u).apply();
                            web.loadUrl(u);
                            Toast.makeText(MainActivity.this, "Saqlandi: " + u, Toast.LENGTH_SHORT).show();
                        }
                    }
                })
                .setNegativeButton("Bekor", null)
                .show();
            return true;
        }
        if (id == 3) {
            try {
                String u = web.getUrl();
                startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(u)));
            } catch (Exception e) { }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
