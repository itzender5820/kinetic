// ─────────────────────────────────────────────────────────────────────────────
//  com.testapp.TrackInfo  —  Immutable track data model (Java)
// ─────────────────────────────────────────────────────────────────────────────
package com.testapp;

/**
 * Immutable snapshot of a track's metadata retrieved from the native engine.
 * Java handles this model so Kotlin can use it via null-safe interop.
 */
public final class TrackInfo {

    public final int    index;
    public final String name;
    public final String artist;
    public final float  durationSec;

    public TrackInfo(int index, String name, String artist, float durationSec) {
        this.index       = index;
        this.name        = name   != null ? name   : "Unknown";
        this.artist      = artist != null ? artist : "Unknown";
        this.durationSec = durationSec > 0f ? durationSec : 1f;
    }

    /** Format duration as MM:SS string. */
    public String formattedDuration() {
        int total = (int) durationSec;
        int min   = total / 60;
        int sec   = total % 60;
        return String.format("%d:%02d", min, sec);
    }

    /** Format position as MM:SS string. */
    public static String formatTime(float sec) {
        int total = (int) sec;
        int min   = total / 60;
        int s     = total % 60;
        return String.format("%d:%02d", min, s);
    }

    @Override
    public String toString() {
        return "TrackInfo{" + index + ", \"" + name + "\", " + formattedDuration() + "}";
    }
}
