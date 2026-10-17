package dev.lyvt.quicky;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

public class TransferService extends Service {
    private PowerManager.WakeLock wakeLock;
    @Override public void onCreate() {
        super.onCreate();
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("quicky-transfers", "Dateiübertragungen", NotificationManager.IMPORTANCE_LOW));
        Intent open = new Intent(this, QuickyActivity.class);
        PendingIntent action = PendingIntent.getActivity(this, 0, open,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification notification = new Notification.Builder(this, "quicky-transfers")
            .setContentTitle("Quicky")
            .setContentText("Sichere Dateiübertragung läuft")
            .setSmallIcon(android.R.drawable.stat_sys_upload)
            .setContentIntent(action).setOngoing(true).build();
        if (Build.VERSION.SDK_INT >= 29)
            startForeground(1, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else startForeground(1, notification);
        PowerManager power = (PowerManager)getSystemService(POWER_SERVICE);
        wakeLock = power.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Quicky:Transfer");
        // Bounded to Android's data-sync service budget. No permanent wake lock.
        wakeLock.acquire(6 * 60 * 60 * 1000L);
    }
    @Override public int onStartCommand(Intent intent, int flags, int id) { return START_NOT_STICKY; }
    @Override public void onDestroy() {
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
