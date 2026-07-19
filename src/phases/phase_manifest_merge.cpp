// ─────────────────────────────────────────────────────────────────────────────
//  phases/phase_manifest_merge.cpp  —  Phase 11: MANIFEST_MERGE
// ─────────────────────────────────────────────────────────────────────────────
#include "phase_manifest_merge.hpp"
#include "../error/error_reporter.hpp"
#include "../error/error_codes.hpp"
#include "../utils/process.hpp"
#include "../kinetic.hpp"
#include <sstream>

namespace kinetic {

static std::string escape_xml_attr(const std::string& s) {
    std::string o; o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&':  o += "&amp;";  break;
            case '<':  o += "&lt;";   break;
            case '>':  o += "&gt;";   break;
            case '"':  o += "&quot;"; break;
            case '\'': o += "&apos;"; break;
            default:   o += c;        break;
        }
    }
    return o;
}

static std::string escape_android_name(const std::string& s) {
    std::string o; o.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '&':  o += "&amp;";  break;
            case '<':  o += "&lt;";   break;
            case '>':  o += "&gt;";   break;
            default:   o += c;        break;
        }
    }
    return o;
}

static std::string build_manifest_xml(const KineticConfig& cfg) {
    std::ostringstream x;
    x << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    x << "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n";
    x << "    android:versionCode=\"" << cfg.version_code << "\"\n";
    x << "    android:versionName=\"" << escape_xml_attr(cfg.version_name) << "\"";

    if (!cfg.package_name.empty())
        x << "\n    package=\"" << escape_xml_attr(cfg.package_name) << "\"";

    x << ">\n";
    if (cfg.min_sdk >= 1) {
        x << "    <uses-sdk android:minSdkVersion=\"" << cfg.min_sdk
          << "\" android:targetSdkVersion=\"" << cfg.target_sdk << "\" />\n";
    }

    x << "\n    <application\n";
    if (!cfg.app_name.empty())
        x << "        android:label=\"" << escape_xml_attr(cfg.app_name) << "\"\n";
    x << "        android:hasCode=\"true\"\n";
    x << "        android:name=\"android.app.Application\"\n";
    x << "    >\n";

    std::string activity = cfg.package_name + "/.MainActivity";
    x << "        <activity android:name=\"" << escape_android_name(activity) << "\"\n";
    x << "            android:exported=\"true\"\n";
    x << "            android:configChanges=\"orientation|screenSize|keyboardHidden\">\n";
    x << "            <intent-filter>\n";
    x << "                <action android:name=\"android.intent.action.MAIN\" />\n";
    x << "                <category android:name=\"android.intent.category.LAUNCHER\" />\n";
    x << "            </intent-filter>\n";
    x << "        </activity>\n";

    x << "    </application>\n";
    x << "</manifest>\n";
    return x.str();
}

void PhaseManifestMerge::execute(PhaseContext& ctx) {
    const fs::path src_manifest = ctx.project_root / "src" / "main" / "AndroidManifest.xml";
    const fs::path dst_manifest = ctx.dirs.staging_dir / "AndroidManifest.xml";

    ctx.timer.start("MANIFEST_MERGE");

    std::error_code ec;
    fs::create_directories(dst_manifest.parent_path(), ec);

    phase_log("MANIFEST_MERGE", "Generating app manifest ...");
    std::string manifest_xml = build_manifest_xml(ctx.cfg);

    fs::path tmp_manifest = dst_manifest.string() + ".tmp";
    {
        std::ofstream ofs(tmp_manifest);
        if (!ofs) fatal("MANIFEST_MERGE", err::PKG_004,
            "Failed to write manifest", tmp_manifest.string(),
            "Check disk permissions in build/intermediates/");
        ofs << manifest_xml;
    }

    if (fs::exists(dst_manifest)) fs::remove(dst_manifest, ec);
    fs::rename(tmp_manifest, dst_manifest, ec);
    if (ec) fatal("MANIFEST_MERGE", err::PKG_004,
        "Failed to move manifest into place", dst_manifest.string(),
        "Check disk permissions in build/intermediates/");

    phase_log("MANIFEST_MERGE", "AndroidManifest.xml generated ✓");
    ctx.timer.stop(true, "manifest merged");
}

} // namespace kinetic
