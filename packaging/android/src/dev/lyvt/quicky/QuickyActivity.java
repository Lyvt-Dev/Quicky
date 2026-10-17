package dev.lyvt.quicky;

import org.qtproject.qt.android.bindings.QtActivity;
import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.WindowManager;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.util.concurrent.Executors;

public class QuickyActivity extends QtActivity {
    private static final int EXPORT_FILE = 7101;
    private String exportPath;
    public String displayName(String uri) {
        try (Cursor cursor = getContentResolver().query(Uri.parse(uri),
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) return cursor.getString(0);
        } catch (Exception ignored) { }
        return "";
    }
    public void keepAwake(boolean enabled) {
        runOnUiThread(() -> {
            if (enabled) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
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
