package dev.lyvt.quicky;

import org.qtproject.qt.android.bindings.QtActivity;
import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.net.wifi.WifiManager;
import android.content.Context;
import androidx.core.content.ContextCompat;
import android.os.Build;
import android.Manifest;
import android.content.pm.PackageManager;
import android.provider.OpenableColumns;
import android.view.WindowManager;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.util.concurrent.Executors;
import com.google.zxing.integration.android.IntentIntegrator;
import com.google.zxing.integration.android.IntentResult;
import java.util.concurrent.atomic.AtomicReference;

public class QuickyActivity extends QtActivity {
    private static final int EXPORT_FILE = 7101;
    private String exportPath;
    private WifiManager.MulticastLock multicastLock;
    private boolean serviceActive = false;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        WifiManager wifi = (WifiManager)getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wifi != null) {
            multicastLock = wifi.createMulticastLock("Quicky discovery");
            multicastLock.setReferenceCounted(false);
            multicastLock.acquire();
        }
    }
    @Override public void onDestroy() {
        if (multicastLock != null && multicastLock.isHeld()) multicastLock.release();
        super.onDestroy();
    }
    private final AtomicReference<String> scanResult = new AtomicReference<>("");
    public void scanInvitation() {
        runOnUiThread(() -> new IntentIntegrator(this)
            .setDesiredBarcodeFormats(IntentIntegrator.QR_CODE)
            .setBeepEnabled(false).setOrientationLocked(false)
            .setPrompt("Quicky-Einladung scannen")
            .initiateScan());
    }
    public String takeScanResult() {
        return scanResult.getAndSet("");
    }
    public String displayName(String uri) {
        try (Cursor cursor = getContentResolver().query(Uri.parse(uri),
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) return cursor.getString(0);
        } catch (Exception ignored) { }
        return "";
    }
    public void keepAwake(boolean enabled) {
        runOnUiThread(() -> {
            if (serviceActive == enabled) return;
            serviceActive = enabled;
            Intent service = new Intent(this, TransferService.class);
            if (enabled) {
                if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
                    requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 7102);
                try { ContextCompat.startForegroundService(this, service); }
                catch (Exception error) {
                    serviceActive = false;
                    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    Toast.makeText(this, "Bitte Quicky während des Transfers geöffnet lassen", Toast.LENGTH_LONG).show();
                }
            } else {
                stopService(service);
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }
    public void exportFile(String path) {
        runOnUiThread(() -> {
            File file = new File(path);
            // Only export received files inside this app's private storage.
            try {
                String base = getFilesDir().getCanonicalPath() + File.separator;
                if (!file.getCanonicalPath().startsWith(base) || !file.isFile()) return;
                exportPath = file.getCanonicalPath();
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("application/octet-stream");
                intent.putExtra(Intent.EXTRA_TITLE, file.getName());
                startActivityForResult(intent, EXPORT_FILE);
            } catch (Exception error) {
                Toast.makeText(this, "Datei kann nicht exportiert werden", Toast.LENGTH_LONG).show();
            }
        });
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        IntentResult scan = IntentIntegrator.parseActivityResult(request, result, data);
        if (scan != null) {
            scanResult.set(scan.getContents() == null ? "\n" : scan.getContents());
            return;
        }
        if (request != EXPORT_FILE || result != Activity.RESULT_OK || data == null || exportPath == null) return;
        final String source = exportPath;
        final Uri destination = data.getData();
        exportPath = null;
        java.util.concurrent.ExecutorService executor = Executors.newSingleThreadExecutor();
        executor.execute(() -> {
            String message;
            try (FileInputStream in = new FileInputStream(source);
                 OutputStream out = getContentResolver().openOutputStream(destination, "w")) {
                if (out == null) throw new java.io.IOException("No output stream");
                byte[] buffer = new byte[256 * 1024];
                int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                out.flush();
                message = "Datei gespeichert";
            } catch (Exception error) { message = "Speichern fehlgeschlagen"; }
            final String finalMessage = message;
            runOnUiThread(() -> Toast.makeText(this, finalMessage, Toast.LENGTH_LONG).show());
            executor.shutdown();
        });
    }
}
