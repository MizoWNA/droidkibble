package dk;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.view.Surface;
import android.view.SurfaceControl;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.FileReader;
import java.lang.reflect.Method;

/**
 * Paints the phone's status on the screen while the Android UI is off.
 *
 * SurfaceFlinger and the hardware composer keep running in headless mode, so this only has to ask
 * SurfaceFlinger for a layer and draw on it. It runs under app_process, needs no system_server, and
 * reads /data/adb/phoneserver/status.json, which phoneserverd keeps up to date.
 *
 *   status.jar [--seconds N] [--status FILE]
 */
public class Status {
    static final int W = 1080, H = 2340;
    static final String LIME = "#dfff4f";

    public static void main(String[] args) throws Exception {
        long endAt = Long.MAX_VALUE;
        String file = "/data/adb/phoneserver/status.json";
        for (int i = 0; i < args.length - 1; i++) {
            if (args[i].equals("--seconds")) endAt = System.currentTimeMillis() + Long.parseLong(args[i + 1]) * 1000;
            if (args[i].equals("--status")) file = args[i + 1];
        }

        SurfaceControl sc = new SurfaceControl.Builder()
                .setName("droidkibble").setBufferSize(W, H).setFormat(PixelFormat.RGBA_8888).build();
        SurfaceControl.Transaction t = new SurfaceControl.Transaction();
        try {   // hidden API: put the layer on the built-in display (layer stack 0)
            Method m = SurfaceControl.Transaction.class.getMethod("setLayerStack", SurfaceControl.class, int.class);
            m.invoke(t, sc, 0);
        } catch (Throwable e) {
            System.err.println("setLayerStack: " + e);
        }
        t.setLayer(sc, Integer.MAX_VALUE).setVisibility(sc, true).apply();
        Surface surface = new Surface(sc);
        System.out.println("layer created, drawing");

        int n = 0;
        while (System.currentTimeMillis() < endAt) {
            Canvas c = surface.lockCanvas(null);
            try {
                draw(c, readJson(file), n++);
            } finally {
                surface.unlockCanvasAndPost(c);
            }
            Thread.sleep(1000);
        }
        surface.release();
        sc.release();
    }

    static JSONObject readJson(String file) {
        try (BufferedReader r = new BufferedReader(new FileReader(file))) {
            StringBuilder sb = new StringBuilder();
            for (String l; (l = r.readLine()) != null; ) sb.append(l);
            return new JSONObject(sb.toString());
        } catch (Exception e) {
            return null;
        }
    }

    static void draw(Canvas c, JSONObject j, int tick) throws Exception {
        c.drawColor(Color.parseColor("#0c0c10"));
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setTypeface(Typeface.MONOSPACE);

        p.setColor(Color.parseColor(LIME));
        p.setTextSize(64);
        c.drawText("* DROIDKIBBLE *", 60, 160, p);
        p.setColor(Color.GRAY);
        p.setTextSize(40);
        c.drawText("frame " + tick + "   " + new java.util.Date(), 60, 230, p);

        if (j == null) {
            p.setColor(Color.RED);
            p.setTextSize(56);
            c.drawText("no status.json", 60, 400, p);
            return;
        }
        JSONObject b = j.getJSONObject("battery"), n = j.getJSONObject("network"), w = j.getJSONObject("watchdog");
        int pct = b.getInt("percent");

        p.setColor(pct < 20 ? Color.RED : Color.parseColor(LIME));
        p.setTextSize(300);
        c.drawText(pct + "%", 60, 640, p);
        p.setColor(Color.parseColor("#2a2a32"));
        c.drawRect(60, 700, 1020, 760, p);
        p.setColor(pct < 20 ? Color.RED : Color.parseColor(LIME));
        c.drawRect(60, 700, 60 + 960f * pct / 100f, 760, p);

        p.setColor(Color.WHITE);
        p.setTextSize(52);
        int y = 900;
        String[] lines = {
            b.getString("state") + "  " + b.getDouble("temp_c") + " C",
            "ip   " + n.getString("ip"),
            "gw   " + n.getString("gateway") + (n.getBoolean("gateway_reachable") ? "  ok" : "  DOWN"),
            "wdog " + w.getString("device") + "  fed " + w.getInt("fed_ago_s") + "s",
            "up   " + j.getInt("uptime_s") / 60 + " min",
        };
        for (String l : lines) {
            c.drawText(l, 60, y, p);
            y += 90;
        }
    }
}
