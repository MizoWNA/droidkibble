package dk;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

/**
 * What pages read from. status.json (written by phoneserverd) is the root. Any other *.json file in the
 * extras directory is added under its file name, so server-side information can be shown later just by
 * writing a file: extras/nodhd.json becomes "nodhd.tasks_today" and so on. Lookups use dotted paths and
 * never throw; a missing value gives the default.
 */
public final class Data {
    private JSONObject root = new JSONObject();
    private final String statusFile, extrasDir;
    public boolean ok;

    public Data(String statusFile, String extrasDir) {
        this.statusFile = statusFile;
        this.extrasDir = extrasDir;
    }

    public void refresh() {
        JSONObject r = read(statusFile);
        ok = r != null;
        if (r == null) r = new JSONObject();
        File[] files = extrasDir == null ? null : new File(extrasDir).listFiles();
        if (files != null) {
            for (File f : files) {
                String n = f.getName();
                if (!n.endsWith(".json")) continue;
                JSONObject o = read(f.getPath());
                if (o != null) try { r.put(n.substring(0, n.length() - 5), o); } catch (Exception ignored) { }
            }
        }
        root = r;
    }

    private static JSONObject read(String path) {
        try (BufferedReader r = new BufferedReader(new FileReader(path))) {
            StringBuilder sb = new StringBuilder();
            for (String l; (l = r.readLine()) != null; ) sb.append(l);
            return new JSONObject(sb.toString());
        } catch (Exception e) {
            return null;
        }
    }

    private Object at(String path) {
        Object o = root;
        for (String k : path.split("\\.")) {
            if (!(o instanceof JSONObject)) return null;
            o = ((JSONObject) o).opt(k);
        }
        return o;
    }

    public String str(String path, String def) {
        Object o = at(path);
        return o == null || o instanceof JSONObject ? def : String.valueOf(o);
    }

    public double num(String path, double def) {
        Object o = at(path);
        return o instanceof Number ? ((Number) o).doubleValue() : def;
    }

    public boolean bool(String path, boolean def) {
        Object o = at(path);
        return o instanceof Boolean ? (Boolean) o : def;
    }

    public boolean has(String path) {
        return at(path) != null;
    }
}
