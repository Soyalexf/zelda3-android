package com.dishii.zelda3;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.zip.CRC32;
import java.util.Locale;

/**
 * Entry point. Makes sure the game data is in place before the engine starts.
 *
 * It exists because of a real dead end on Android 11 and later: the game reads
 * its files from Android/data/, which the system file manager can no longer
 * browse. Asking people to put a ROM there meant asking them for a PC and a
 * USB cable. Here the system file picker is used instead, so the ROM can be
 * chosen from Downloads and copied into place by the app itself.
 */
public class SetupActivity extends Activity {

    private static final int kPickRom = 1;

    private static boolean isSpanish() {
        return Locale.getDefault().getLanguage().equals("es");
    }

    private static String t(String en, String es) {
        return isSpanish() ? es : en;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Deploy the bundled files first. MainActivity used to do this after
        // starting SDL, which raced with the engine reading them.
        deployBundledFiles();

        if (hasGameData()) {
            startGame();
            return;
        }
        showRomPrompt();
    }

    private File filesDir() {
        File d = getExternalFilesDir(null);
        return d != null ? d : getFilesDir();
    }

    /**
     * Either the prebuilt asset file, or a ROM the bundled patch can actually
     * use. The ROM is checked rather than merely counted: a wrong file copied
     * here would otherwise look like valid data forever, sending every launch
     * straight to the engine's error screen with no way back to the picker.
     */
    private boolean hasGameData() {
        File dir = filesDir();
        if (new File(dir, "zelda3_assets.dat").exists())
            return true;
        File rom = new File(dir, "zelda3.sfc");
        // A ROM alone is not enough: a language has to have been picked, which
        // is what puts the chosen patch in place.
        return rom.exists() && romMatchesPatch(rom)
                && new File(dir, "zelda3_assets.bps").exists();
    }

    /** The expected ROM checksum, read from the bundled BPS patch's footer. */
    private long expectedRomCrc() {
        // Both patches expect the same source ROM, so either answers this.
        File bps = new File(filesDir(), "zelda3_assets_en.bps");
        try (RandomAccessFile f = new RandomAccessFile(bps, "r")) {
            if (f.length() < 12)
                return -1;
            f.seek(f.length() - 12);
            byte[] b = new byte[4];
            f.readFully(b);
            return ((long) (b[0] & 0xff)) | ((long) (b[1] & 0xff) << 8)
                    | ((long) (b[2] & 0xff) << 16) | ((long) (b[3] & 0xff) << 24);
        } catch (Exception e) {
            return -1;
        }
    }

    private boolean romMatchesPatch(File rom) {
        long want = expectedRomCrc();
        if (want < 0)
            return true;  // no patch to check against: let the engine decide
        return crc32(rom) == want;
    }

    private static long crc32(File f) {
        CRC32 crc = new CRC32();
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0)
                crc.update(buf, 0, n);
            return crc.getValue();
        } catch (IOException e) {
            return -1;
        }
    }

    private void deployBundledFiles() {
        File dir = filesDir();
        new File(dir, "saves" + File.separator + "ref").mkdirs();
        try {
            AssetCopyUtil.copyAssetsToExternal(this, "saves/ref",
                    dir.getAbsolutePath() + "/saves/ref");
        } catch (IOException e) {
            // Reference saves are optional; not worth blocking startup for.
        }
        copyAssetIfMissing("zelda3.ini", new File(dir, "zelda3.ini"));
        // Both language patches are deployed; picking a language copies one of
        // them to the name the engine looks for.
        copyAssetIfMissing("zelda3_assets_en.bps", new File(dir, "zelda3_assets_en.bps"));
        copyAssetIfMissing("zelda3_assets_es.bps", new File(dir, "zelda3_assets_es.bps"));
    }

    private void copyAssetIfMissing(String assetName, File target) {
        if (target.exists())
            return;
        try (InputStream in = getAssets().open(assetName);
             FileOutputStream out = new FileOutputStream(target)) {
            copy(in, out);
        } catch (IOException e) {
            // Nothing useful to do here: the engine will report what's missing.
        }
    }

    private static void copy(InputStream in, FileOutputStream out) throws IOException {
        byte[] buf = new byte[16384];
        int n;
        while ((n = in.read(buf)) > 0)
            out.write(buf, 0, n);
    }

    private void showRomPrompt() {
        int pad = dp(24);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(pad, pad, pad, pad);
        root.setBackgroundColor(Color.parseColor("#101018"));

        TextView title = new TextView(this);
        title.setText(t("Choose your ROM", "Elige tu ROM"));
        title.setTextColor(Color.parseColor("#F0D060"));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 26);
        title.setGravity(Gravity.CENTER);

        TextView body = new TextView(this);
        body.setText(t(
                "This game needs your own copy of The Legend of Zelda: "
                        + "A Link to the Past (USA).\n\n"
                        + "Pick the .sfc or .smc file and it will be copied into place. "
                        + "The game builds everything else by itself.",
                "Este juego necesita tu propia copia de The Legend of Zelda: "
                        + "A Link to the Past (USA).\n\n"
                        + "Elige el archivo .sfc o .smc y se copiará donde corresponde. "
                        + "El juego genera lo demás por su cuenta."));
        body.setTextColor(Color.parseColor("#C0C0CC"));
        body.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        body.setGravity(Gravity.CENTER);
        body.setPadding(0, dp(16), 0, dp(28));

        Button pick = new Button(this);
        pick.setText(t("SELECT ROM", "SELECCIONAR ROM"));
        pick.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openPicker();
            }
        });

        root.addView(title);
        root.addView(body);
        root.addView(pick);
        setContentView(root);
    }

    private int dp(int v) {
        return (int) (v * getResources().getDisplayMetrics().density);
    }

    private void openPicker() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        // Most pickers report an unknown MIME type for .sfc, so filtering by
        // type would hide the very file we are asking for.
        i.setType("*/*");
        // The picker opens on "Recent", which is empty for a file that was just
        // downloaded. Point it at Downloads, where the ROM most likely is.
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            Uri downloads = android.provider.DocumentsContract.buildDocumentUri(
                    "com.android.externalstorage.documents", "primary:Download");
            i.putExtra("android.provider.extra.INITIAL_URI", downloads);
        }
        try {
            startActivityForResult(i, kPickRom);
        } catch (Exception e) {
            Toast.makeText(this, t("No file picker available",
                    "No hay selector de archivos"), Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != kPickRom || result != RESULT_OK || data == null
                || data.getData() == null)
            return;
        if (!copyRom(data.getData())) {
            Toast.makeText(this, t("Could not read that file",
                    "No se pudo leer ese archivo"), Toast.LENGTH_LONG).show();
            return;
        }
        File rom = new File(filesDir(), "zelda3.sfc");
        if (!romMatchesPatch(rom)) {
            // Delete it, or the next launch would take this for valid data and
            // never offer the picker again.
            rom.delete();
            Toast.makeText(this, t(
                    "That is not the right ROM. It must be A Link to the Past (USA).",
                    "Esa no es la ROM correcta. Debe ser A Link to the Past (USA)."),
                    Toast.LENGTH_LONG).show();
            return;
        }
        showLanguageChoice();
    }

    /** Asked once, after the ROM is accepted: which language to build. */
    private void showLanguageChoice() {
        int pad = dp(24);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(pad, pad, pad, pad);
        root.setBackgroundColor(Color.parseColor("#101018"));

        TextView title = new TextView(this);
        title.setText(t("Game language", "Idioma del juego"));
        title.setTextColor(Color.parseColor("#F0D060"));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 26);
        title.setGravity(Gravity.CENTER);

        TextView body = new TextView(this);
        body.setText(t("This is the language of the game's own text. "
                        + "It is built from your ROM now, so it cannot be changed later "
                        + "without setting up again.",
                "Este es el idioma de los textos del juego. Se genera ahora a partir "
                        + "de tu ROM, así que no se puede cambiar después sin volver a "
                        + "configurar."));
        body.setTextColor(Color.parseColor("#C0C0CC"));
        body.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        body.setGravity(Gravity.CENTER);
        body.setPadding(0, dp(16), 0, dp(28));

        Button en = new Button(this);
        en.setText("ENGLISH");
        en.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { applyLanguage(false); }
        });
        Button es = new Button(this);
        es.setText("ESPAÑOL");
        es.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { applyLanguage(true); }
        });

        root.addView(title);
        root.addView(body);
        root.addView(en);
        root.addView(es);
        setContentView(root);
    }

    private void applyLanguage(boolean spanish) {
        File dir = filesDir();
        File chosen = new File(dir, spanish ? "zelda3_assets_es.bps"
                                            : "zelda3_assets_en.bps");
        File target = new File(dir, "zelda3_assets.bps");
        try (InputStream in = new java.io.FileInputStream(chosen);
             FileOutputStream out = new FileOutputStream(target)) {
            copy(in, out);
        } catch (IOException e) {
            Toast.makeText(this, t("Could not prepare that language",
                    "No se pudo preparar ese idioma"), Toast.LENGTH_LONG).show();
            return;
        }
        setIniLanguage(spanish ? "es" : null);
        startGame();
    }

    /**
     * Sets, or comments out, the Language key in the .ini. The engine reads it
     * from the [General] section, so the existing line is rewritten in place
     * rather than appended, which would land it in whatever section is last.
     */
    private void setIniLanguage(String lang) {
        File ini = new File(filesDir(), "zelda3.ini");
        try {
            byte[] raw = new byte[(int) ini.length()];
            try (java.io.FileInputStream in = new java.io.FileInputStream(ini)) {
                int got = 0;
                while (got < raw.length) {
                    int n = in.read(raw, got, raw.length - got);
                    if (n <= 0) break;
                    got += n;
                }
            }
            String text = new String(raw, "UTF-8");
            String eol = text.contains("\r\n") ? "\r\n" : "\n";
            String[] lines = text.split("\r?\n", -1);
            String want = lang == null ? "# Language = de" : "Language = " + lang;
            boolean done = false;
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < lines.length; i++) {
                String line = lines[i];
                String trimmed = line.trim();
                if (!done && (trimmed.startsWith("Language")
                        || trimmed.startsWith("# Language"))) {
                    line = want;
                    done = true;
                }
                sb.append(line);
                if (i < lines.length - 1)
                    sb.append(eol);
            }
            try (FileOutputStream out = new FileOutputStream(ini)) {
                out.write(sb.toString().getBytes("UTF-8"));
            }
        } catch (Exception e) {
            // Leaving the default language is better than failing to start.
        }
    }

    private boolean copyRom(Uri uri) {
        File target = new File(filesDir(), "zelda3.sfc");
        try (InputStream in = getContentResolver().openInputStream(uri);
             FileOutputStream out = new FileOutputStream(target)) {
            if (in == null)
                return false;
            // Many .smc dumps carry a 512-byte copier header the engine does
            // not expect. Peek at the size to decide whether to skip it.
            byte[] head = new byte[512];
            int got = 0;
            while (got < head.length) {
                int n = in.read(head, got, head.length - got);
                if (n <= 0)
                    break;
                got += n;
            }
            long size = sizeOf(uri);
            boolean headered = size > 0 && (size % 1024) == 512;
            if (!headered)
                out.write(head, 0, got);
            copy(in, out);
            return target.length() > 0;
        } catch (Exception e) {
            return false;
        }
    }

    private long sizeOf(Uri uri) {
        try (android.os.ParcelFileDescriptor fd =
                     getContentResolver().openFileDescriptor(uri, "r")) {
            return fd != null ? fd.getStatSize() : -1;
        } catch (Exception e) {
            return -1;
        }
    }

    private void startGame() {
        startActivity(new Intent(this, MainActivity.class));
        finish();
    }
}
