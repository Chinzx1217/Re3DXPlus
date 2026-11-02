package com.wmmt.port;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Environment;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.VideoView;

import java.io.File;

public class MainActivity extends Activity implements SurfaceHolder.Callback, SensorEventListener {
  private static final int STORAGE_PERMISSION_REQUEST = 1001;

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
  private String saveDir;
  private SensorManager sensors;
  private Sensor gravity;
  private Surface pendingSurface;
  private boolean nativeStarted;

  @Override protected void onCreate(Bundle b) {
    super.onCreate(b);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
    if (android.os.Build.VERSION.SDK_INT >= 28)
      getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

    root = new FrameLayout(this);
    game = new SurfaceView(this);
    game.getHolder().addCallback(this);
    game.setOnTouchListener((v, e) -> { if (nativeStarted) touch(e); return true; });
    root.addView(game);
    setContentView(root);
    hideBars();

    sensors = (SensorManager) getSystemService(SENSOR_SERVICE);
    gravity = sensors.getDefaultSensor(Sensor.TYPE_GRAVITY);

    if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
      requestPermissions(new String[] { Manifest.permission.READ_EXTERNAL_STORAGE }, STORAGE_PERMISSION_REQUEST);
    } else {
      startGameWithStorage();
    }
  }

  @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if (requestCode != STORAGE_PERMISSION_REQUEST) return;
    if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      startGameWithStorage();
    } else {
      fail("Storage permission is required to read the game data folder. Allow Files and media access, then reopen the game.");
    }
  }

  private File volumeRoot(File appSpecificDir) {
    File dir = appSpecificDir;
    for (int i = 0; i < 4 && dir != null; ++i) dir = dir.getParentFile();
    return dir;
  }

  private File findGameData() {
    File[] appDirs = getExternalFilesDirs(null);
    if (appDirs != null) {
      for (File appDir : appDirs) {
        if (appDir == null || !Environment.isExternalStorageRemovable(appDir)) continue;
        File volumeRoot = volumeRoot(appDir);
        if (volumeRoot == null) continue;
        File candidate = new File(volumeRoot, "data");
        if (candidate.exists() && candidate.isDirectory()) {
          if (candidate.canRead() && new File(candidate, "menu.bin").isFile()) return candidate;
          fail("The SD card contains a data folder, but it cannot be read or menu.bin is missing: " + candidate.getAbsolutePath());
          return null;
        }
      }
    }

    File primaryRoot = Environment.getExternalStorageDirectory();
    File internalData = new File(primaryRoot, "data");
    if (internalData.isDirectory() && internalData.canRead() && new File(internalData, "menu.bin").isFile())
      return internalData;

    fail("Game data was not found. Put the complete game folder at SD card root/data first. If it is not there, put it at internal storage root/data. The folder must contain menu.bin.");
    return null;
  }

  private void startGameWithStorage() {
    if (nativeStarted) return;
    File data = findGameData();
    if (data == null) return;

    File save = new File(getExternalFilesDir(null), "save");
    if (!save.exists() && !save.mkdirs()) {
      fail("Cannot create the game's save folder: " + save.getAbsolutePath());
      return;
    }

    dataDir = data.getAbsolutePath();
    saveDir = save.getAbsolutePath();
    nativeStart(this, dataDir, saveDir);
    nativeStarted = true;
    if (pendingSurface != null) nativeSurface(pendingSurface, root.getWidth(), root.getHeight());
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

  @Override public void surfaceChanged(SurfaceHolder h, int fmt, int w, int ht) {
    pendingSurface = h.getSurface();
    if (nativeStarted) nativeSurface(pendingSurface, w, ht);
  }

  @Override public void surfaceDestroyed(SurfaceHolder h) {
    pendingSurface = null;
    if (nativeStarted) nativeSurface(null, 0, 0);
  }

  @Override protected void onPause() {
    super.onPause();
    if (nativeStarted) nativePause(true);
    if (sensors != null) sensors.unregisterListener(this);
    if (bgm != null) bgm.pause();
    if (movie != null) movie.pause();
  }

  @Override protected void onResume() {
    super.onResume();
    hideBars();
    if (nativeStarted) nativePause(false);
    if (gravity != null) sensors.registerListener(this, gravity, SensorManager.SENSOR_DELAY_GAME);
    if (bgm != null) bgm.start();
    if (movie != null) movie.start();
  }

  @Override public void onTrimMemory(int level) {
    super.onTrimMemory(level);
    if (nativeStarted && level >= TRIM_MEMORY_RUNNING_LOW) nativeMemoryWarning();
  }

  @Override public void onSensorChanged(SensorEvent e) {
    if (!nativeStarted || !nativeWantsTilt()) return;
    float gx = e.values[0], gy = e.values[1];
    int rot = getWindowManager().getDefaultDisplay().getRotation();
    float right = rot == Surface.ROTATION_270 ? gy : -gy, down = rot == Surface.ROTATION_270 ? -gx : gx;
    float g2 = right * right + down * down;
    nativeTilt(g2 < 0.04f * 96.2f ? 0.0f : (float) Math.atan2(-right, down));
  }

  @Override public void onAccuracyChanged(Sensor s, int a) {}

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
      movie.setOnTouchListener((v, e) -> {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) nativePressStart();
        return true;
      });
      FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
          FrameLayout.LayoutParams.MATCH_PARENT, android.view.Gravity.CENTER);
      int w = root.getWidth(), h = root.getHeight();
      if (name.equals("intro.mp4") && w * 3 > h * 4) {
        lp.width = w;
        lp.height = w * 3 / 4;
      }
      root.addView(movie, lp);
      movie.start();
    });
  }

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
