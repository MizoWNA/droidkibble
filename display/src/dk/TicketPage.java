package dk;

import android.graphics.Paint;
import android.graphics.Path;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** The phone's status as a letterpress ticket: a ruled grid on the body, and a stub that says how the watchdog is doing. */
public final class TicketPage implements Page {
    public String name() {
        return "ticket";
    }

    // ticket geometry (the screen is 1080 x 2340)
    static final float L = 60, R = 1020, T = 90, B = 2250, PERF = 1650;
    static final float IL = 110, IR = 970, IW = IR - IL, MID = 540;

    public void draw(Ctx x) {
        Ui u = new Ui(x.canvas, x.theme);
        Theme t = x.theme;
        Data d = x.data;
        u.c.drawColor(t.bg);
        Path ticket = Ui.ticket(L, T, R, B, PERF, 34, 9);
        u.paper(ticket);

        long nowS = x.now / 1000;
        boolean have = d.ok && d.has("watchdog.fed_ago_s");
        long age = Math.max(0, nowS - (long) d.num("time", nowS)) + (long) d.num("watchdog.fed_ago_s", 0);
        boolean open = d.bool("watchdog.open", false);
        String mood = !have ? "LOST" : open && age <= 15 ? "FED" : "HUNGRY";
        int upMin = (int) (d.num("uptime_s", 0) / 60);
        Date now = new Date(x.now);

        // ---- header
        String serial = "NO. " + new SimpleDateFormat("yyyyMMdd", Locale.US).format(now) + String.format(Locale.US, "%06d", upMin);
        u.text(serial, IL, 150, 30, t.ink, t.type);
        u.text("ARCH LINUX ARM \u00b7 AARCH64", IR, 150, 24, t.soft, t.type, Paint.Align.RIGHT, 0.05f);
        u.rule(IL, 176, IR, 176, 3, t.ink);
        float ts = u.fit("DROIDKIBBLE", t.block, IW, 230, 0.02f);
        u.text("DROIDKIBBLE", IL - 4, 372, ts, t.ink, t.block, Paint.Align.LEFT, 0.02f);
        u.text("Feed the watchdog, free the RAM.", IL, 434, 46, t.soft, t.script);
        u.rule(IL, 462, IR, 462, 3, t.ink);
        u.rule(IL, 470, IR, 470, 2, t.ink);

        // ---- grid: two columns of cells
        float g0 = 480, ra = 860, rb = 1090, rc = 1330, rd = 1610;
        u.rule(MID, g0, MID, rd, 3, t.ink);
        for (float y : new float[]{ra, rb, rc}) u.rule(IL, y, IR, y, 3, t.ink);
        u.rule(IL, rd, IR, rd, 3, t.ink);
        u.rule(IL, rd + 8, IR, rd + 8, 2, t.ink);
        float lx = IL + 24, rx = MID + 24, cw = 380;

        // date
        u.label("Date", lx, g0 + 52);
        u.text(new SimpleDateFormat("EEEE \u00b7 MMM", Locale.US).format(now).toUpperCase(), lx, g0 + 122, 44, t.ink, t.block, Paint.Align.LEFT, 0.04f);
        String day = new SimpleDateFormat("d", Locale.US).format(now);
        float dw = u.text(day, lx - 4, g0 + 330, 230, t.ink, t.block);
        u.text(new SimpleDateFormat("yyyy", Locale.US).format(now), lx + dw + 18, g0 + 330, 66, t.soft, t.block);

        // battery
        int pct = (int) d.num("battery.percent", -1);
        int bcol = pct >= 0 && pct < 20 ? t.warn : t.ink;
        u.label("Battery", rx, g0 + 52);
        u.text(d.str("battery.state", "?").toUpperCase(), IR - 24, g0 + 52, 26, t.soft, t.label, Paint.Align.RIGHT, 0.1f);
        String ps = pct >= 0 ? String.valueOf(pct) : "--";
        float pw = u.text(ps, rx - 4, g0 + 300, 230, bcol, t.block);
        u.text("%", rx + pw + 6, g0 + 300, 80, bcol, t.block);
        u.text(d.str("battery.temp_c", "?") + "\u00b0C", IR - 24, g0 + 214, 40, t.soft, t.block, Paint.Align.RIGHT, 0.04f);
        u.bar(rx, g0 + 322, cw + 20, 36, Math.max(0, pct) / 100.0, bcol);

        // time
        u.label("Local time", lx, ra + 52);
        u.text(new SimpleDateFormat("HH:mm", Locale.US).format(now), lx - 4, ra + 190, 140, t.ink, t.block);

        // memory
        int used = (int) d.num("memory.used_mb", 0), tot = (int) d.num("memory.total_mb", 1);
        u.label("Memory", rx, ra + 52);
        float mw = u.text(String.valueOf(used), rx - 2, ra + 150, 100, t.ink, t.block);
        u.text("/ " + tot + " MB", rx + mw + 14, ra + 150, 34, t.soft, t.label, Paint.Align.LEFT, 0.06f);
        u.bar(rx, ra + 174, cw + 20, 30, used / (double) Math.max(1, tot), used * 100 / Math.max(1, tot) > 85 ? t.warn : t.ink);

        // network
        boolean up = d.bool("network.gateway_reachable", false);
        u.label("Network", lx, rb + 52);
        u.text(d.str("network.ip", "no address"), lx, rb + 124, 46, t.ink, t.type);
        u.text("via " + d.str("network.gateway", "?"), lx, rb + 172, 36, t.soft, t.type);
        u.dot(lx + 12, rb + 208, 10, up ? t.ink : t.warn);
        u.text(up ? "ROUTER OK" : "ROUTER DOWN", lx + 34, rb + 217, 30, up ? t.ink : t.warn, t.label, Paint.Align.LEFT, 0.12f);

        // watchdog
        u.label("Watchdog", rx, rb + 52);
        float aw = u.text(have ? age + "s" : "--", rx - 2, rb + 150, 100, "FED".equals(mood) ? t.ink : t.warn, t.block);
        u.text("since last feed", rx + aw + 14, rb + 150, 30, t.soft, t.label, Paint.Align.LEFT, 0.06f);
        u.text(d.str("watchdog.device", "no device"), rx, rb + 200, 32, t.soft, t.type);

        // uptime
        u.label("Phone up", lx, rc + 52);
        String upS = upMin >= 60 ? (upMin / 60) + "H " + String.format(Locale.US, "%02d", upMin % 60) + "M" : upMin + " MIN";
        u.text(upS, lx - 4, rc + 190, 110, t.ink, t.block);

        // ssh + dhcp
        u.label("SSH \u00b7 DHCP", rx, rc + 52);
        boolean s1 = d.bool("services.ssh_android_22", false), s2 = d.bool("services.ssh_chroot_2222", false);
        u.dot(rx + 12, rc + 106, 10, s1 ? t.ink : t.warn);
        u.text("android  :22", rx + 34, rc + 117, 36, s1 ? t.ink : t.warn, t.type);
        u.dot(rx + 12, rc + 158, 10, s2 ? t.ink : t.warn);
        u.text("chroot  :2222", rx + 34, rc + 169, 36, s2 ? t.ink : t.warn, t.type);
        int renew = (int) d.num("dhcp.next_renewal_in_s", -1);
        u.text(renew >= 0 ? "DHCP RENEWS IN " + (renew >= 3600 ? renew / 3600 + "H " + (renew % 3600) / 60 + "M" : renew / 60 + " MIN") : "DHCP OFF",
                rx, rc + 226, 28, t.soft, t.label, Paint.Align.LEFT, 0.1f);

        // ---- perforation and stub
        u.dotted(L + 50, PERF, R - 50, 5, t.ink);
        u.label("Status", IL, PERF + 60);
        int wcol = "FED".equals(mood) ? t.ink : t.warn;
        float ws = u.fit(mood, t.block, 470, 250, 0.02f);
        u.text(mood, IL - 4, PERF + 274, ws, wcol, t.block, Paint.Align.LEFT, 0.02f);
        u.rule(IL, PERF + 302, IL + 480, PERF + 302, 4, t.ink);
        u.rule(IL, PERF + 311, IL + 480, PERF + 311, 2, t.ink);
        String line = "FED".equals(mood) ? "Fed every 5 seconds. Good boy." : "HUNGRY".equals(mood) ? "Not fed for " + age + "s. Something is wrong." : "No word from the daemon.";
        u.text(line, IL, PERF + 358, 40, t.accent, t.script);
        u.barcode(IL, PERF + 396, 330, 46, upMin * 31L + 7, t.ink);
        u.text(serial.substring(4), IL, PERF + 476, 24, t.soft, t.type, Paint.Align.LEFT, 0.12f);
        u.stamp(800, PERF + 200, 158, "DROIDKIBBLE \u00b7 WATCHDOG \u00b7 KEEPALIVE \u00b7 ", "FED".equals(mood), !"FED".equals(mood), t.ink);
        u.rule(IL, PERF + 518, IR, PERF + 518, 3, t.ink);
        u.c.save();
        u.c.rotate(-2.5f, 540, B - 34);
        u.text("Kibble runs through my veins.", 540, B - 34, 52, t.accent, t.script, Paint.Align.CENTER, 0f);
        u.c.restore();
    }
}
