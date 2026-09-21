package dk;

import android.graphics.Color;
import android.graphics.Typeface;

/** Colors and typefaces. A page asks the theme for them, so a new look is a new Theme, not new drawing code. */
public final class Theme {
    public final String name;
    public final int bg, paper, ink, soft, accent, warn, highlight, speck;
    public final boolean letterpress;
    public final Typeface block = Typeface.create("sans-serif-condensed", Typeface.BOLD);   // headings, big numbers
    public final Typeface label = Typeface.create("sans-serif-condensed", Typeface.NORMAL); // small caps labels
    public final Typeface script = Typeface.create("serif", Typeface.ITALIC);                 // the handwritten-ish lines
    public final Typeface type = Typeface.create("serif-monospace", Typeface.NORMAL);         // typewriter (Cutive Mono)

    Theme(String name, String bg, String paper, String ink, String soft, String accent, String warn,
          String highlight, String speck, boolean letterpress) {
        this.name = name;
        this.bg = Color.parseColor(bg);
        this.paper = Color.parseColor(paper);
        this.ink = Color.parseColor(ink);
        this.soft = Color.parseColor(soft);
        this.accent = Color.parseColor(accent);
        this.warn = Color.parseColor(warn);
        this.highlight = Color.parseColor(highlight);
        this.speck = Color.parseColor(speck);
        this.letterpress = letterpress;
    }

    /** Cream card stock, blue ink, gold accents. */
    public static Theme paper() {
        return new Theme("paper", "#141516", "#ebe8df", "#1d5ea9", "#7f9dc4", "#d6a03a", "#c2452d",
                "#aaffffff", "#3a3a30", true);
    }

    /** Navy card, cream ink, on true black so an AMOLED panel isn't lit up all night. */
    public static Theme night() {
        return new Theme("night", "#000000", "#0d1830", "#e9e3cf", "#8b93a8", "#e1b04b", "#ff7a5c",
                "#00000000", "#8899bb", false);
    }

    public static Theme byName(String n) {
        return "night".equals(n) ? night() : paper();
    }
}
