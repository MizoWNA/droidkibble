package dk;

import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.view.Surface;
import android.view.SurfaceControl;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;

/**
 * Paints pages on the screen while the Android UI is off.
 *
 * SurfaceFlinger and the hardware composer keep running in headless mode, so this only has to ask
 * SurfaceFlinger for a layer and draw on it. It runs under app_process, needs no system_server, and
 * reads what phoneserverd writes (see Data).
 *
 *   status.jar [--theme paper|night] [--interval SECONDS] [--seconds N] [--status FILE] [--extras DIR]
 *
 * With several pages they take turns, PAGE_SECONDS each. To add a page, implement Page and add it below.
 */
public class Status {
    static final int W = 1080, H = 2340, PAGE_SECONDS = 20;

    public static void main(String[] args) throws Exception {
        long endAt = Long.MAX_VALUE;
        String status = "/data/adb/phoneserver/status.json", extras = "/data/adb/phoneserver/display.d";
        String theme = "paper";
        int interval = 5;
        for (int i = 0; i < args.length - 1; i++) {
            switch (args[i]) {
                case "--seconds": endAt = System.currentTimeMillis() + Long.parseLong(args[i + 1]) * 1000; break;
                case "--status": status = args[i + 1]; break;
                case "--extras": extras = args[i + 1]; break;
                case "--theme": theme = args[i + 1]; break;
                case "--interval": interval = Integer.parseInt(args[i + 1]); break;
            }
        }
        Theme th = Theme.byName(theme);
        Data data = new Data(status, extras);
        List<Page> pages = new ArrayList<>();
        pages.add(new TicketPage());

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
        System.out.println("layer created, theme " + th.name + ", " + pages.size() + " page(s)");

        while (System.currentTimeMillis() < endAt) {
            long now = System.currentTimeMillis();
            data.refresh();
            Page page = pages.get((int) (now / 1000 / PAGE_SECONDS) % pages.size());
            // nudge the picture a few pixels every few minutes so nothing sits on the same pixels for days
            Calendar cal = Calendar.getInstance();
            int m = cal.get(Calendar.MINUTE);
            float dx = (m % 5 - 2) * 3, dy = (m / 5 % 3 - 1) * 3;
            Canvas c = surface.lockCanvas(null);
            try {
                c.translate(dx, dy);
                page.draw(new Page.Ctx(c, W, H, th, data, now));
            } catch (Throwable e) {
                System.err.println("draw failed: " + e);
                e.printStackTrace();
            } finally {
                surface.unlockCanvasAndPost(c);
            }
            Thread.sleep(interval * 1000L);
        }
        surface.release();
        sc.release();
    }
}
