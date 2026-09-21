package dk;

import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

import java.util.Random;

/** The drawing toolkit pages are built from: letterpress text, rules, bars, ticket outline, stamp, barcode. */
public final class Ui {
    public final Canvas c;
    public final Theme t;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private static Bitmap tile;
    private static String tileFor;

    public Ui(Canvas c, Theme t) {
        this.c = c;
        this.t = t;
    }

    // ---- text

    private void setup(Typeface tf, float size, Paint.Align a, float spacing) {
        p.reset();
        p.setAntiAlias(true);
        p.setTypeface(tf);
        p.setTextSize(size);
        p.setTextAlign(a);
        p.setLetterSpacing(spacing);
    }

    public float width(String s, Typeface tf, float size, float spacing) {
        setup(tf, size, Paint.Align.LEFT, spacing);
        return p.measureText(s);
    }

    /** Largest size (up to max) at which s fits in maxW. */
    public float fit(String s, Typeface tf, float maxW, float max, float spacing) {
        float w = width(s, tf, 100, spacing);
        return Math.min(max, 100f * maxW / w);
    }

    /** Text with a pressed-in edge on themes that ask for it. Returns the drawn width. */
    public float text(String s, float x, float y, float size, int color, Typeface tf, Paint.Align a, float spacing) {
        setup(tf, size, a, spacing);
        float w = p.measureText(s);
        if (t.letterpress) {
            p.setColor(t.highlight);
            c.drawText(s, x + 1.8f, y + 1.8f, p);
        }
        p.setColor(color);
        c.drawText(s, x, y, p);
        return w;
    }

    public float text(String s, float x, float y, float size, int color, Typeface tf) {
        return text(s, x, y, size, color, tf, Paint.Align.LEFT, 0f);
    }

    /** Small gold caps label. */
    public void label(String s, float x, float y) {
        text(s.toUpperCase(), x, y, 30, t.accent, t.label, Paint.Align.LEFT, 0.16f);
    }

    // ---- lines and shapes

    public void rule(float x1, float y1, float x2, float y2, float w, int color) {
        p.reset();
        p.setAntiAlias(true);
        p.setColor(color);
        p.setStrokeWidth(w);
        c.drawLine(x1, y1, x2, y2, p);
    }

    public void dotted(float x1, float y, float x2, float w, int color) {
        p.reset();
        p.setAntiAlias(true);
        p.setColor(color);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(w);
        p.setPathEffect(new DashPathEffect(new float[]{w * 2.2f, w * 2.6f}, 0));
        c.drawLine(x1, y, x2, y, p);
    }

    public void dot(float cx, float cy, float r, int color) {
        p.reset();
        p.setAntiAlias(true);
        p.setColor(color);
        c.drawCircle(cx, cy, r, p);
    }

    /** Outlined bar filled to frac (0..1). */
    public void bar(float x, float y, float w, float h, double frac, int color) {
        p.reset();
        p.setAntiAlias(true);
        p.setColor(color);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(4);
        c.drawRect(x, y, x + w, y + h, p);
        p.setStyle(Paint.Style.FILL);
        float f = (float) Math.max(0, Math.min(1, frac));
        c.drawRect(x + 7, y + 7, x + 7 + (w - 14) * f, y + h - 7, p);
    }

    // ---- ticket

    /** A ticket outline: scalloped short edges, half-round bites at the perforation, both sides. */
    public static Path ticket(float l, float tp, float r, float b, float perfY, float bite, float scallop) {
        Path shape = new Path();
        shape.addRoundRect(new RectF(l, tp, r, b), 12, 12, Path.Direction.CW);
        Path cut = new Path();
        float step = scallop * 2.7f;
        for (float x = l + step; x < r - step / 2; x += step) {
            cut.addCircle(x, tp, scallop, Path.Direction.CW);
            cut.addCircle(x, b, scallop, Path.Direction.CW);
        }
        cut.addCircle(l, perfY, bite, Path.Direction.CW);
        cut.addCircle(r, perfY, bite, Path.Direction.CW);
        shape.op(cut, Path.Op.DIFFERENCE);
        return shape;
    }

    private Bitmap tile() {
        if (tile != null && t.name.equals(tileFor)) return tile;
        Bitmap b = Bitmap.createBitmap(256, 256, Bitmap.Config.ARGB_8888);
        Canvas cv = new Canvas(b);
        Paint q = new Paint(Paint.ANTI_ALIAS_FLAG);
        Random rnd = new Random(7);
        for (int i = 0; i < 950; i++) {          // paper grain
            q.setColor(Color.argb(10 + rnd.nextInt(30), Color.red(t.speck), Color.green(t.speck), Color.blue(t.speck)));
            cv.drawCircle(rnd.nextInt(256), rnd.nextInt(256), 0.5f + rnd.nextFloat() * 1.0f, q);
        }
        q.setStrokeWidth(1);
        for (int i = 0; i < 110; i++) {          // a few fibres
            q.setColor(Color.argb(14, Color.red(t.speck), Color.green(t.speck), Color.blue(t.speck)));
            float x = rnd.nextInt(256), y = rnd.nextInt(256), a = rnd.nextFloat() * 6.28f;
            cv.drawLine(x, y, x + (float) Math.cos(a) * 9, y + (float) Math.sin(a) * 9, q);
        }
        tile = b;
        tileFor = t.name;
        return b;
    }

    /** The card: soft shadow, paper color, grain, and a faint light-to-dark wash. */
    public void paper(Path shape) {
        RectF r = new RectF();
        shape.computeBounds(r, true);
        p.reset();
        p.setAntiAlias(true);
        p.setColor(t.paper);
        p.setShadowLayer(36, 0, 18, 0xb0000000);
        c.drawPath(shape, p);
        p.clearShadowLayer();
        c.save();
        c.clipPath(shape);
        p.reset();
        p.setShader(new BitmapShader(tile(), Shader.TileMode.REPEAT, Shader.TileMode.REPEAT));
        c.drawRect(r, p);
        p.reset();
        p.setShader(new LinearGradient(0, r.top, 0, r.bottom, Color.argb(t.letterpress ? 26 : 10, 255, 255, 255),
                Color.argb(t.letterpress ? 30 : 40, 0, 0, 0), Shader.TileMode.CLAMP));
        c.drawRect(r, p);
        c.restore();
    }

    // ---- stamp and barcode

    /** A round stamp: two rings, text running round the edge, a dog in the middle. */
    public void stamp(float cx, float cy, float r, String ring, boolean tongue, boolean worried, int color) {
        p.reset();
        p.setAntiAlias(true);
        p.setColor(color);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(5);
        c.drawCircle(cx, cy, r, p);
        c.drawCircle(cx, cy, r - 50, p);

        float size = 33, tr = r - 34;                       // text baseline radius
        Path path = new Path();
        path.addCircle(cx, cy, tr, Path.Direction.CW);
        setup(t.block, size, Paint.Align.LEFT, 0.06f);
        float circ = (float) (2 * Math.PI * tr);
        int reps = Math.max(1, (int) (circ / p.measureText(ring)));
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < reps; i++) sb.append(ring);
        float natural = p.measureText(sb.toString());
        p.setLetterSpacing(0.06f + (circ - natural) / sb.length() / size);
        p.setColor(color);
        c.drawTextOnPath(sb.toString(), path, 0, size * 0.32f, p);

        dog(cx, cy + 4, (r - 50) / 105f, color, tongue, worried);
    }

    /** Line-art dog head, about 200 units across at scale 1. */
    public void dog(float cx, float cy, float s, int color, boolean tongue, boolean worried) {
        c.save();
        c.translate(cx, cy);
        c.scale(s, s);
        p.reset();
        p.setAntiAlias(true);
        p.setColor(color);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(6);
        p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeJoin(Paint.Join.ROUND);
        c.drawOval(new RectF(-60, -50, 60, 56), p);                       // head
        Path ear = new Path();                                            // floppy ears
        ear.moveTo(-46, -40);
        ear.cubicTo(-92, -68, -104, 8, -72, 44);
        ear.cubicTo(-62, 22, -54, -10, -46, -40);
        c.drawPath(ear, p);
        c.save();
        c.scale(-1, 1);
        c.drawPath(ear, p);
        c.restore();
        c.drawOval(new RectF(-28, 6, 28, 46), p);                         // snout
        p.setStyle(Paint.Style.FILL);
        c.drawOval(new RectF(-12, 8, 12, 22), p);                         // nose
        c.drawCircle(-26, -12, 7.5f, p);                                  // eyes
        c.drawCircle(26, -12, 7.5f, p);
        p.setStyle(Paint.Style.STROKE);
        Path mouth = new Path();
        mouth.moveTo(0, 22);
        mouth.lineTo(0, 30);
        mouth.moveTo(-17, 34);
        mouth.quadTo(-2, 46, 0, 30);
        mouth.quadTo(2, 46, 17, 34);
        c.drawPath(mouth, p);
        if (worried) {
            c.drawLine(-40, -32, -14, -24, p);
            c.drawLine(40, -32, 14, -24, p);
        }
        if (tongue) {
            p.setStyle(Paint.Style.FILL);
            p.setColor(t.accent);
            c.drawRoundRect(new RectF(-9, 38, 9, 66), 9, 9, p);
        }
        c.restore();
    }

    public void barcode(float x, float y, float w, float h, long seed, int color) {
        p.reset();
        p.setColor(color);
        Random r = new Random(seed);
        float cx = x;
        while (cx < x + w) {
            float bw = 2 + r.nextInt(6);
            if (cx + bw > x + w) break;
            c.drawRect(cx, y, cx + bw, y + h, p);
            cx += bw + 2 + r.nextInt(4);
        }
    }
}
