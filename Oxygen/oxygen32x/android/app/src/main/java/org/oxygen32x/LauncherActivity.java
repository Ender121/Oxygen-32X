package org.oxygen32x;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Lets the user pick their own 32X ROM (a raw file or a .zip containing one),
 * copies it to the app's private storage as rom.32x and starts the game.
 * No ROM is bundled with the app.
 */
public class LauncherActivity extends Activity {
    private static final int REQ_PICK = 1;
    private static final int MAX_ROM = 8 * 1024 * 1024;

    private Button play;
    private TextView status;

    private File romFile() { return new File(getFilesDir(), "rom.32x"); }

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.BLACK);
        root.setPadding(48, 48, 48, 48);

        TextView title = new TextView(this);
        title.setText("Oxygen-32X");
        title.setTextColor(Color.WHITE);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        status = new TextView(this);
        status.setTextColor(Color.LTGRAY);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, 24, 0, 24);
        root.addView(status);

        play = new Button(this);
        play.setText("Jugar");
        play.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { startGame(); }
        });
        root.addView(play);

        Button pick = new Button(this);
        pick.setText("Elegir ROM de 32X (.32x / .bin / .zip)");
        pick.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { pickRom(); }
        });
        root.addView(pick);

        setContentView(root);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refresh();
    }

    private void refresh() {
        boolean have = romFile().isFile();
        play.setEnabled(have);
        status.setText(have ? "ROM lista." : "Elige tu propia ROM de 32X para empezar.\nNo se incluye ninguna ROM.");
    }

    private void startGame() {
        startActivity(new Intent(this, GameActivity.class));
    }

    private void pickRom() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, REQ_PICK);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_PICK || res != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        status.setText("Importando…");
        new Thread(new Runnable() {
            public void run() {
                final String err = importRom(uri);
                runOnUiThread(new Runnable() {
                    public void run() {
                        if (err != null) Toast.makeText(LauncherActivity.this, err, Toast.LENGTH_LONG).show();
                        refresh();
                        if (err == null) startGame();
                    }
                });
            }
        }).start();
    }

    /** Returns null on success, otherwise a message for the user. */
    private String importRom(Uri uri) {
        try {
            byte[] rom = readRom(uri);
            if (rom == null) return "No se pudo leer el archivo.";
            if (rom.length < 0x800 || !isMars(rom)) return "Eso no parece una ROM de 32X (falta 'SEGA 32X').";
            File tmp = new File(getFilesDir(), "rom.32x.tmp");
            OutputStream o = new FileOutputStream(tmp);
            try { o.write(rom); } finally { o.close(); }
            File dst = romFile();
            if (dst.exists() && !dst.delete()) return "No se pudo reemplazar la ROM anterior.";
            if (!tmp.renameTo(dst)) return "No se pudo guardar la ROM.";
            return null;
        } catch (IOException e) {
            return "Error al importar: " + e.getMessage();
        }
    }

    private static boolean isMars(byte[] r) {
        final byte[] sig = "SEGA 32X".getBytes();
        for (int i = 0; i < sig.length; i++) if (r[0x100 + i] != sig[i]) return false;
        return true;
    }

    private byte[] readRom(Uri uri) throws IOException {
        InputStream raw = getContentResolver().openInputStream(uri);
        if (raw == null) return null;
        BufferedInputStream in = new BufferedInputStream(raw);
        try {
            in.mark(4);
            byte[] magic = new byte[2];
            int n = in.read(magic);
            in.reset();
            if (n == 2 && magic[0] == 'P' && magic[1] == 'K') {
                ZipInputStream zin = new ZipInputStream(in);
                ZipEntry e;
                while ((e = zin.getNextEntry()) != null) {
                    if (e.isDirectory()) continue;
                    byte[] d = readAll(zin);
                    if (d.length >= 0x800 && isMars(d)) return d;
                }
                return null;
            }
            return readAll(in);
        } finally {
            in.close();
        }
    }

    private static byte[] readAll(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(1 << 22);
        byte[] buf = new byte[65536];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
            if (out.size() > MAX_ROM) throw new IOException("archivo demasiado grande");
        }
        return out.toByteArray();
    }
}
