package com.wmmt.port;

import android.app.Activity;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.VideoView;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/* The Android side of the port, as main.m is the iOS one: the game draws on a SurfaceView
 * from its own thread (src/main_android.c), the attract movies play in a VideoView over it,
 * the BGM in a MediaPlayer (both decode the AAC/H.264 in hardware); touches, the gravity for
 * steering by tilt and the lifecycle go down to the native side. */
public class MainActivity extends Activity implements SurfaceHolder.Callback, SensorEventListener {
  static { System.loadLibrary("wmmt"); }

  static native void nativeStart(MainActivity a, String dataDir, String saveDir);
  static native void nativeSurface(Surface s, int w, int h);
  static native void nativeTouch(int id, int phase, float x, float y);
  static native void nativePause(boolean p);
  static native void nativeTilt(float radians);
  static native boolean nativeWantsTilt();
  static native void nativeMovieDone();
  static native void nativePressStart();
  static native void nativeMemoryWarning();

  private FrameLayout root;
  private SurfaceView game;
  private VideoView movie;
  private MediaPlayer bgm;
  private String dataDir;
  private SensorManager sensors;
  private Sensor gravity;

  @Override protected void onCreate(Bundle b) {
    super.onCreate(b);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
    if (android.os.Build.VERSION.SDK_INT >= 28)
      getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    root = new FrameLayout(this);
    game = new SurfaceView(this);
    game.getHolder().addCallback(this);
    game.setOnTouchListener((v, e) -> { touch(e); return true; });
    root.addView(game);
    setContentView(root);
    hideBars();
    /* Prefer complete game data on internal storage, then an SD card, then a USB drive.
     * Each candidate is an app-specific external-files directory, so native code can use
     * ordinary POSIX paths without a Storage Access Framework content URI. */
    File[] storageDirs = getExternalFilesDirs(null);
    File internalFiles = storageDirs != null && storageDirs.length > 0 && storageDirs[0] != null
        ? storageDirs[0] : getExternalFilesDir(null);
    File saveData = new File(internalFiles, "wmmt");
    saveData.mkdirs();
    dataDir = findGameDataDir(storageDirs, internalFiles);
    sensors = (SensorManager) getSystemService(SENSOR_SERVICE);
    gravity = sensors.getDefaultSensor(Sensor.TYPE_GRAVITY);
    nativeStart(this, dataDir, saveData.getAbsolutePath());
  }

  private static final class DataLocation {
    final File directory;
    final int priority;

    DataLocation(File directory, int priority) {
      this.directory = directory;
      this.priority = priority;
    }
  }

  /* A data set is usable only when its main Tokyo course, starting car, menu and
   * the cars/sound resource folders are present. A folder by itself is not enough. */
  private boolean hasGameData(File directory) {
    return new File(directory, "tokyo.wpk").isFile()
        && new File(directory, "fd3s.wpk").isFile()
        && new File(directory, "menu.bin").isFile()
        && new File(directory, "cars").isDirectory()
        && new File(directory, "sound").isDirectory();
  }

  /* getExternalFilesDirs() lists the primary volume first and removable volumes after it.
   * On Android versions that expose volume UUIDs, use the system description to put SD
   * cards before USB drives. Unknown removable volumes are checked after both. */
  private int removableStoragePriority(File appFilesDir) {
    File volumeRoot = appFilesDir;
    for (int i = 0; i < 4 && volumeRoot != null; ++i) volumeRoot = volumeRoot.getParentFile();
    if (volumeRoot == null) return 3;

    StorageManager manager = (StorageManager) getSystemService(STORAGE_SERVICE);
    if (manager == null) return 3;
    for (StorageVolume volume : manager.getStorageVolumes()) {
      String uuid = volume.getUuid();
      if (uuid == null || !volumeRoot.getName().equalsIgnoreCase(uuid)) continue;
      String description = volume.getDescription(this).toLowerCase(Locale.US);
      if (description.contains("usb")) return 2;
      if (description.contains("sd") || description.contains("card")) return 1;
      return volume.isRemovable() ? 3 : 0;
    }
    return 3;
  }

  private String findGameDataDir(File[] storageDirs, File internalFiles) {
    List<DataLocation> candidates = new ArrayList<>();
    if (storageDirs != null) {
      for (int i = 0; i < storageDirs.length; ++i) {
        File files = storageDirs[i];
        if (files == null) continue;
        int priority = i == 0 ? 0 : removableStoragePriority(files);
        candidates.add(new DataLocation(new File(files, "wmmt"), priority));
      }
    }
    if (candidates.isEmpty() && internalFiles != null)
      candidates.add(new DataLocation(new File(internalFiles, "wmmt"), 0));

    Collections.sort(candidates, new Comparator<DataLocation>() {
      @Override public int compare(DataLocation a, DataLocation b) {
        return Integer.compare(a.priority, b.priority);
      }
    });

    for (DataLocation candidate : candidates) {
      if (hasGameData(candidate.directory)) {
        android.util.Log.i("WMMT3DX", "Game data selected: " + candidate.directory.getAbsolutePath());
        return candidate.directory.getAbsolutePath();
      }
    }

    /* Keep the existing internal path as the failure fallback, so the native loader can
     * report its normal missing-resource error when no complete copy is available. */
    File fallback = new File(internalFiles, "wmmt");
    android.util.Log.w("WMMT3DX", "No complete game data set found; using " + fallback.getAbsolutePath());
    return fallback.getAbsolutePath();
  }

  private void hideBars() {
    getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
  }

  @Override public void onWindowFocusChanged(boolean f) {
    super.onWindowFocusChanged(f);
    if (f) hideBars();
  }

  private void touch(MotionEvent e) {
    int a = e.getActionMasked();
    if (a == MotionEvent.ACTION_MOVE) {
      for (int i = 0; i < e.getPointerCount(); ++i) nativeTouch(e.getPointerId(i), 1, e.getX(i), e.getY(i));
      return;
    }
    int phase = a == MotionEvent.ACTION_DOWN || a == MotionEvent.ACTION_POINTER_DOWN ? 0
        : a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_POINTER_UP ? 2 : a == MotionEvent.ACTION_CANCEL ? 3 : -1;
    if (phase < 0) return;
    if (phase == 3) {
      for (int i = 0; i < e.getPointerCount(); ++i) nativeTouch(e.getPointerId(i), 2, e.getX(i), e.getY(i));
      return;
    }
    int i = e.getActionIndex();
    nativeTouch(e.getPointerId(i), phase, e.getX(i), e.getY(i));
  }

  @Override public void surfaceCreated(SurfaceHolder h) {}
  @Override public void surfaceChanged(SurfaceHolder h, int fmt, int w, int ht) { nativeSurface(h.getSurface(), w, ht); }
  @Override public void surfaceDestroyed(SurfaceHolder h) { nativeSurface(null, 0, 0); }

  @Override protected void onPause() {
    super.onPause();
    nativePause(true);
    sensors.unregisterListener(this);
    if (bgm != null) bgm.pause();
    if (movie != null) movie.pause();
  }

  @Override protected void onResume() {
    super.onResume();
    hideBars();
    nativePause(false);
    if (gravity != null) sensors.registerListener(this, gravity, SensorManager.SENSOR_DELAY_GAME);
    if (bgm != null) bgm.start();
    if (movie != null) movie.start();
  }

  @Override public void onTrimMemory(int level) {
    super.onTrimMemory(level);
    if (level >= TRIM_MEMORY_RUNNING_LOW) nativeMemoryWarning();
  }

  /* the phone turned like a wheel (main.m's tilt): gravity's angle in the screen's plane from
   * its down, clockwise positive; nothing when it lies flat. In landscape the screen's right is
   * the sensor's -y (rotation 90) or +y (270) and its down the sensor's +x or -x. */
  @Override public void onSensorChanged(SensorEvent e) {
    if (!nativeWantsTilt()) return;
    float gx = e.values[0], gy = e.values[1];
    int rot = getWindowManager().getDefaultDisplay().getRotation();
    float right = rot == Surface.ROTATION_270 ? gy : -gy, down = rot == Surface.ROTATION_270 ? -gx : gx;
    float g2 = right * right + down * down;
    nativeTilt(g2 < 0.04f * 96.2f ? 0.0f : (float) Math.atan2(-right, down));
  }
  @Override public void onAccuracyChanged(Sensor s, int a) {}

  /* ---- called by the game's thread ---- */

  /* the attract movie (data dir's intro.mp4 etc.), or none for null */
  void showMovie(final String name) {
    runOnUiThread(() -> {
      if (movie != null) {
        movie.stopPlayback();
        root.removeView(movie);
        movie = null;
      }
      if (name == null) return;
      File f = new File(dataDir, name);
      if (!f.exists()) {
        nativeMovieDone();
        return;
      }
      movie = new VideoView(this);
      movie.setZOrderMediaOverlay(true);
      movie.setVideoPath(f.getAbsolutePath());
      movie.setOnCompletionListener(mp -> nativeMovieDone());
      movie.setOnErrorListener((mp, what, extra) -> { nativeMovieDone(); return true; });
      /* a tap on the movie is the arcade's "press the gas" */
      movie.setOnTouchListener((v, e) -> {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) nativePressStart();
        return true;
      });
      FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
          FrameLayout.LayoutParams.MATCH_PARENT, android.view.Gravity.CENTER);
      /* the intro's picture is letterboxed in its 4:3 (640x326 at y 76): on a wider screen it
       * goes the screen's width, its black bands off the top and bottom; playdemo and tutorial
       * fill their 4:3 and stay so */
      int w = root.getWidth(), h = root.getHeight();
      if (name.equals("intro.mp4") && w * 3 > h * 4) {
        lp.width = w;
        lp.height = w * 3 / 4;
      }
      root.addView(movie, lp);
      movie.start();
    });
  }

  /* data dir's sound/bgm/<name>.m4a over and over, or nothing for null */
  void playBgm(final String name) {
    runOnUiThread(() -> {
      if (bgm != null) {
        bgm.release();
        bgm = null;
      }
      if (name == null) return;
      File f = new File(dataDir, "sound/bgm/" + name + ".m4a");
      if (!f.exists()) return;
      try {
        bgm = new MediaPlayer();
        bgm.setDataSource(f.getAbsolutePath());
        bgm.setLooping(true);
        bgm.prepare();
        bgm.start();
      } catch (Exception ex) {
        bgm = null;
      }
    });
  }

  /* the BGM's level (the story ending's fade out; a new track starts at full) */
  void bgmLevel(final float level) {
    runOnUiThread(() -> {
      if (bgm != null) bgm.setVolume(level, level);
    });
  }

  void fail(final String msg) {
    runOnUiThread(() -> new android.app.AlertDialog.Builder(this).setTitle("WMMT3DX").setMessage(msg)
        .setPositiveButton("OK", null).show());
  }
}
