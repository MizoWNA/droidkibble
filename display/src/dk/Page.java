package dk;

import android.graphics.Canvas;

/** One screen. draw() paints the whole frame from ctx.data; keep it free of side effects. */
public interface Page {
    String name();

    void draw(Ctx ctx);

    final class Ctx {
        public final Canvas canvas;
        public final int w, h;
        public final Theme theme;
        public final Data data;
        public final long now;

        public Ctx(Canvas canvas, int w, int h, Theme theme, Data data, long now) {
            this.canvas = canvas; this.w = w; this.h = h; this.theme = theme; this.data = data; this.now = now;
        }
    }
}
