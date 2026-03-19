// ─────────────────────────────────────────────────────────────────────────────
//  gl/Renderer.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "Renderer.hpp"
#include <GLES2/gl2ext.h>
#include <numbers>
#include <vector>
#include <cstring>

namespace testapp {

// ─────────────────────────────────────────────────────────────────────────────
//  GLSL Shaders  (ES 2.0 — no version directive for max compatibility)
// ─────────────────────────────────────────────────────────────────────────────

// ── Background ────────────────────────────────────────────────────────────────
static constexpr std::string_view kBgVert = R"(
attribute vec2 a_pos;
varying   vec2 v_uv;
void main() {
    v_uv        = a_pos * 0.5 + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static constexpr std::string_view kBgFrag = R"(
precision mediump float;
varying vec2  v_uv;
uniform float u_time;
uniform float u_bass;
uniform vec2  u_res;          // viewport in pixels

void main() {
    vec2 uv = v_uv;
    float d = length(uv - 0.5) * 2.0;

    // Deep space gradient
    vec3 col = mix(vec3(0.04, 0.04, 0.14),
                   vec3(0.00, 0.00, 0.04), d);

    // Bass-pulse radial glow
    float glow = smoothstep(0.6, 0.0, d) * u_bass * 0.35;
    col += vec3(0.0, 0.10, 0.40) * glow;

    // Subtle hex-grid overlay
    vec2 hex = uv * 14.0;
    float hx = abs(fract(hex.x) - 0.5);
    float hy = abs(fract(hex.y) - 0.5);
    float grid = step(0.45, max(hx, hy));
    col += vec3(0.04) * grid * (0.5 + 0.5 * u_bass);

    // Slow colour shift over time
    float shift = 0.04 * sin(u_time * 0.3);
    col.r += shift;
    col.b -= shift * 0.5;

    gl_FragColor = vec4(col, 1.0);
}
)";

// ── Spectrum Bars ─────────────────────────────────────────────────────────────
static constexpr std::string_view kBarsVert = R"(
attribute vec2  a_pos;
attribute float a_amp;
varying   float v_amp;
varying   float v_r;          // normalised radial distance (0=inner,1=tip)
uniform   float u_inv_aspect; // H/W  — makes circles circular on portrait screens
void main() {
    v_amp       = a_amp;
    v_r         = a_amp;      // outer verts get amp passed from CPU
    gl_Position = vec4(a_pos.x * u_inv_aspect, a_pos.y, 0.0, 1.0);
}
)";

static constexpr std::string_view kBarsFrag = R"(
precision mediump float;
varying float v_amp;
varying float v_r;
uniform float u_time;
uniform float u_bass;

void main() {
    // Colour palette: navy → cyan → magenta (low → high amplitude)
    vec3 c0 = vec3(0.05, 0.20, 0.80);
    vec3 c1 = vec3(0.00, 0.90, 0.80);
    vec3 c2 = vec3(1.00, 0.10, 0.60);
    vec3 col;
    if (v_amp < 0.5)
        col = mix(c0, c1, v_amp * 2.0);
    else
        col = mix(c1, c2, (v_amp - 0.5) * 2.0);

    // Tip glow: brighten bar tips
    col *= 0.6 + 0.4 * v_r;

    // Bass shimmer
    col += vec3(0.08) * u_bass * sin(u_time * 8.0 + v_amp * 6.0);

    // Minimum brightness so silent bars stay visible
    col = max(col, vec3(0.03, 0.03, 0.10));

    gl_FragColor = vec4(col, 1.0);
}
)";

// ── Center Orb ────────────────────────────────────────────────────────────────
static constexpr std::string_view kOrbVert = R"(
attribute vec2  a_pos;
uniform   float u_inv_aspect;
uniform   float u_scale;
varying   vec2  v_local;
void main() {
    v_local     = a_pos;
    gl_Position = vec4(a_pos.x * u_inv_aspect * u_scale,
                       a_pos.y * u_scale, 0.0, 1.0);
}
)";

static constexpr std::string_view kOrbFrag = R"(
precision mediump float;
varying vec2  v_local;
uniform float u_time;
uniform float u_bass;
uniform float u_playing;   // 1.0=playing, 0.0=paused

void main() {
    float d = length(v_local);
    if (d > 1.0) discard;

    // Core glow
    float core = 1.0 - smoothstep(0.0, 0.85, d);
    // Edge ring
    float ring = smoothstep(0.72, 0.80, d) - smoothstep(0.80, 0.88, d);

    // Animated swirl
    float angle = atan(v_local.y, v_local.x);
    float swirl = 0.5 + 0.5 * sin(angle * 6.0 - u_time * 4.0);

    // Colour
    vec3 play_col = mix(vec3(0.0, 0.5, 1.0), vec3(0.2, 1.0, 0.8), u_bass);
    vec3 pause_col = vec3(0.3, 0.3, 0.4);
    vec3 col = mix(pause_col, play_col, u_playing);

    // Bass pulse
    col += vec3(0.0, 0.0, 0.3) * u_bass * core;

    float alpha = core * (0.6 + 0.4 * swirl) + ring * 0.9;
    gl_FragColor = vec4(col, alpha);
}
)";

// ── Progress Ring ─────────────────────────────────────────────────────────────
static constexpr std::string_view kRingVert = R"(
attribute vec2  a_pos;
attribute float a_norm;   // normalised arc position 0–1
uniform   float u_inv_aspect;
varying   float v_norm;
void main() {
    v_norm      = a_norm;
    gl_Position = vec4(a_pos.x * u_inv_aspect, a_pos.y, 0.0, 1.0);
}
)";

static constexpr std::string_view kRingFrag = R"(
precision mediump float;
varying float v_norm;
uniform float u_progress;   // 0–1
uniform float u_time;

void main() {
    // Only draw the arc up to u_progress
    if (v_norm > u_progress) discard;

    float t    = v_norm / max(u_progress, 0.001);
    vec3  col  = mix(vec3(0.1, 0.4, 1.0), vec3(1.0, 0.2, 0.6), t);
    float anim = 0.7 + 0.3 * sin(u_time * 3.0 - v_norm * 10.0);
    gl_FragColor = vec4(col * anim, 0.9);
}
)";

// ─────────────────────────────────────────────────────────────────────────────
//  Renderer implementation
// ─────────────────────────────────────────────────────────────────────────────

Renderer::Renderer()  = default;
Renderer::~Renderer() {
    // All GL resources released by the GL thread when surface is destroyed
}

// ── compile_shader ────────────────────────────────────────────────────────────
GLuint Renderer::compile_shader(GLenum type, std::string_view src) {
    GLuint sh = glCreateShader(type);
    if (!sh) return 0;
    const char* ptr = src.data();
    auto len = static_cast<GLint>(src.size());
    glShaderSource(sh, 1, &ptr, &len);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(sh, sizeof(buf), nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

// ── link_program ──────────────────────────────────────────────────────────────
GLuint Renderer::link_program(GLuint vert, GLuint frag) {
    if (!vert || !frag) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        LOGE("Program link error: %s", buf);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ── build_program ─────────────────────────────────────────────────────────────
GLuint Renderer::build_program(std::string_view vsrc, std::string_view fsrc) {
    GLuint v = compile_shader(GL_VERTEX_SHADER,   vsrc);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    GLuint p = link_program(v, f);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// ── build_bg_geometry ─────────────────────────────────────────────────────────
void Renderer::build_bg_geometry() {
    // Full-screen quad: 2 triangles, positions in NDC
    static constexpr float kQuad[] = {
        -1.f,-1.f,  1.f,-1.f,  1.f, 1.f,
        -1.f,-1.f,  1.f, 1.f, -1.f, 1.f
    };
    if (!vbo_bg_) glGenBuffers(1, &vbo_bg_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bg_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
}

// ── build_orb_geometry ────────────────────────────────────────────────────────
// Unit circle fan for the center orb (drawn with GL_TRIANGLE_FAN).
void Renderer::build_orb_geometry(int segments) {
    const float r = 0.12f;  // radius in y-NDC units
    std::vector<float> pts;
    pts.reserve((segments + 2) * 2);
    pts.push_back(0.f); pts.push_back(0.f);  // center
    for (int i = 0; i <= segments; ++i) {
        const float a = 2.f * std::numbers::pi_v<float> * i / segments;
        pts.push_back(r * std::cos(a));
        pts.push_back(r * std::sin(a));
    }
    if (!vbo_orb_) glGenBuffers(1, &vbo_orb_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_orb_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(pts.size() * sizeof(float)),
                 pts.data(), GL_STATIC_DRAW);
}

// ── build_ring_geometry ───────────────────────────────────────────────────────
// Progress ring: thin quad-strip around a circle.
// Each vertex: x, y, normalised_arc_position (0–1)
void Renderer::build_ring_geometry(int segments) {
    const float r_inner = 0.78f;
    const float r_outer = 0.83f;
    std::vector<float> pts;
    pts.reserve(segments * 12);  // 2 triangles/seg × 3 verts × (x,y,norm)

    for (int i = 0; i < segments; ++i) {
        const float n0 = static_cast<float>(i)   / segments;
        const float n1 = static_cast<float>(i+1) / segments;
        const float a0 = 2.f * std::numbers::pi_v<float> * n0 - std::numbers::pi_v<float> / 2.f;
        const float a1 = 2.f * std::numbers::pi_v<float> * n1 - std::numbers::pi_v<float> / 2.f;

        auto add = [&](float r, float a, float n) {
            pts.push_back(r * std::cos(a));
            pts.push_back(r * std::sin(a));
            pts.push_back(n);
        };
        // Triangle 1: inner0, outer0, outer1
        add(r_inner, a0, n0); add(r_outer, a0, n0); add(r_outer, a1, n1);
        // Triangle 2: inner0, outer1, inner1
        add(r_inner, a0, n0); add(r_outer, a1, n1); add(r_inner, a1, n1);
    }
    if (!vbo_ring_) glGenBuffers(1, &vbo_ring_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ring_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(pts.size() * sizeof(float)),
                 pts.data(), GL_STATIC_DRAW);
}

// ── update_bars_vbo ───────────────────────────────────────────────────────────
// Rebuild spectrum bar quads every frame from current spectrum data.
// Each bar: 2 triangles (6 verts), each vert = (x, y, amplitude)
void Renderer::update_bars_vbo(const std::array<float, kVisBands>& spec) {
    constexpr float kInnerR   = 0.18f;
    constexpr float kMaxBarH  = 0.55f;
    constexpr float kAngleStep = 2.f * std::numbers::pi_v<float> / kVisBands;
    constexpr float kHalfAngle = kAngleStep * 0.38f;  // gap between bars

    std::vector<float> verts;
    verts.reserve(kVisBands * 6 * 3);

    for (int i = 0; i < kVisBands; ++i) {
        const float amp     = spec[i];
        const float theta   = i * kAngleStep;
        const float outerR  = kInnerR + amp * kMaxBarH;

        const float a0 = theta - kHalfAngle;
        const float a1 = theta + kHalfAngle;

        // 4 corner positions (inner / outer, left / right of bar)
        const float xi0 = kInnerR * std::cos(a0), yi0 = kInnerR * std::sin(a0);
        const float xi1 = kInnerR * std::cos(a1), yi1 = kInnerR * std::sin(a1);
        const float xo0 = outerR  * std::cos(a0), yo0 = outerR  * std::sin(a0);
        const float xo1 = outerR  * std::cos(a1), yo1 = outerR  * std::sin(a1);

        // Inner verts get amp=0 (for gradient), outer verts get amp=amp
        auto push = [&](float x, float y, float a) {
            verts.push_back(x); verts.push_back(y); verts.push_back(a);
        };
        // Triangle 1
        push(xi0, yi0, 0.f); push(xo0, yo0, amp); push(xo1, yo1, amp);
        // Triangle 2
        push(xi0, yi0, 0.f); push(xo1, yo1, amp); push(xi1, yi1, 0.f);
    }

    if (!vbo_bars_) glGenBuffers(1, &vbo_bars_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bars_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
}

// ── on_surface_created ────────────────────────────────────────────────────────
bool Renderer::on_surface_created() {
    glClearColor(0.f, 0.f, 0.04f, 1.f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    prog_bg_   = build_program(kBgVert,   kBgFrag);
    prog_bars_ = build_program(kBarsVert, kBarsFrag);
    prog_orb_  = build_program(kOrbVert,  kOrbFrag);
    prog_ring_ = build_program(kRingVert, kRingFrag);

    if (!prog_bg_ || !prog_bars_ || !prog_orb_ || !prog_ring_) {
        LOGE("Renderer: failed to compile one or more shader programs");
        return false;
    }

    build_bg_geometry();
    build_orb_geometry(64);
    build_ring_geometry(128);

    LOGI("Renderer: surface created — all shaders compiled");
    return true;
}

// ── on_surface_changed ────────────────────────────────────────────────────────
void Renderer::on_surface_changed(int w, int h) {
    width_      = w ? w : 1;
    height_     = h ? h : 1;
    inv_aspect_ = static_cast<float>(h) / static_cast<float>(w ? w : 1);
    glViewport(0, 0, w, h);
    LOGI("Renderer: surface changed %d×%d  inv_aspect=%.3f", w, h, inv_aspect_);
}

// ── draw_background ───────────────────────────────────────────────────────────
void Renderer::draw_background(float time_sec, float bass) {
    glUseProgram(prog_bg_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bg_);

    GLint a_pos = glGetAttribLocation(prog_bg_, "a_pos");
    if (a_pos < 0) return;
    glEnableVertexAttribArray(static_cast<GLuint>(a_pos));
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform1f(glGetUniformLocation(prog_bg_, "u_time"), time_sec);
    glUniform1f(glGetUniformLocation(prog_bg_, "u_bass"), bass);
    glUniform2f(glGetUniformLocation(prog_bg_, "u_res"),
                static_cast<float>(width_), static_cast<float>(height_));

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(static_cast<GLuint>(a_pos));
}

// ── draw_bars ─────────────────────────────────────────────────────────────────
void Renderer::draw_bars(const std::array<float, kVisBands>& spec, float time_sec) {
    // Compute bass (average of first 4 bands)
    float bass = 0.f;
    for (int i = 0; i < 4; ++i) bass += spec[i];
    bass /= 4.f;

    update_bars_vbo(spec);
    glUseProgram(prog_bars_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bars_);

    constexpr int kStride = 3 * sizeof(float);
    GLint a_pos = glGetAttribLocation(prog_bars_, "a_pos");
    GLint a_amp = glGetAttribLocation(prog_bars_, "a_amp");
    if (a_pos < 0 || a_amp < 0) return;
    glEnableVertexAttribArray(static_cast<GLuint>(a_pos));
    glEnableVertexAttribArray(static_cast<GLuint>(a_amp));
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(0));
    glVertexAttribPointer(a_amp, 1, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(2 * sizeof(float)));

    glUniform1f(glGetUniformLocation(prog_bars_, "u_inv_aspect"), inv_aspect_);
    glUniform1f(glGetUniformLocation(prog_bars_, "u_time"),       time_sec);
    glUniform1f(glGetUniformLocation(prog_bars_, "u_bass"),       bass);

    glDrawArrays(GL_TRIANGLES, 0, kVisBands * 6);
    glDisableVertexAttribArray(static_cast<GLuint>(a_pos));
    glDisableVertexAttribArray(static_cast<GLuint>(a_amp));
}

// ── draw_orb ──────────────────────────────────────────────────────────────────
void Renderer::draw_orb(float bass, float time_sec, bool is_playing) {
    // Orb radius pulses with bass
    const float scale = 1.0f + bass * 0.3f;

    glUseProgram(prog_orb_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_orb_);

    GLint a_pos = glGetAttribLocation(prog_orb_, "a_pos");
    if (a_pos < 0) return;
    glEnableVertexAttribArray(static_cast<GLuint>(a_pos));
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform1f(glGetUniformLocation(prog_orb_, "u_inv_aspect"), inv_aspect_);
    glUniform1f(glGetUniformLocation(prog_orb_, "u_scale"),      scale);
    glUniform1f(glGetUniformLocation(prog_orb_, "u_time"),       time_sec);
    glUniform1f(glGetUniformLocation(prog_orb_, "u_bass"),       bass);
    glUniform1f(glGetUniformLocation(prog_orb_, "u_playing"),    is_playing ? 1.f : 0.f);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 66);  // center + 64 + 1 wrap-around
    glDisableVertexAttribArray(static_cast<GLuint>(a_pos));
}

// ── draw_ring ─────────────────────────────────────────────────────────────────
void Renderer::draw_ring(float position_norm) {
    constexpr int kRingSegs = 128;
    glUseProgram(prog_ring_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ring_);

    constexpr int kStride = 3 * sizeof(float);
    GLint a_pos  = glGetAttribLocation(prog_ring_, "a_pos");
    GLint a_norm = glGetAttribLocation(prog_ring_, "a_norm");
    if (a_pos < 0 || a_norm < 0) return;
    glEnableVertexAttribArray(static_cast<GLuint>(a_pos));
    glEnableVertexAttribArray(static_cast<GLuint>(a_norm));
    glVertexAttribPointer(a_pos,  2, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(0));
    glVertexAttribPointer(a_norm, 1, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(2 * sizeof(float)));

    glUniform1f(glGetUniformLocation(prog_ring_, "u_inv_aspect"), inv_aspect_);
    glUniform1f(glGetUniformLocation(prog_ring_, "u_progress"),   position_norm);
    glUniform1f(glGetUniformLocation(prog_ring_, "u_time"),       0.f); // unused here

    glDrawArrays(GL_TRIANGLES, 0, kRingSegs * 6);
    glDisableVertexAttribArray(static_cast<GLuint>(a_pos));
    glDisableVertexAttribArray(static_cast<GLuint>(a_norm));
}

// ── draw_frame ────────────────────────────────────────────────────────────────
void Renderer::draw_frame(const std::array<float, kVisBands>& spectrum,
                           float position_norm,
                           bool  is_playing,
                           float time_sec) {
    // Safety: if shader programs are not compiled yet, skip the frame.
    // This can happen if on_surface_created() hasn't been called yet.
    if (!prog_bg_ || !prog_bars_ || !prog_orb_ || !prog_ring_) {
        glClearColor(0.f, 0.f, 0.04f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    glClear(GL_COLOR_BUFFER_BIT);

    // Compute bass energy (average of first 4 spectrum bands)
    float bass = 0.f;
    for (int i = 0; i < 4; ++i) bass += spectrum[i];
    bass /= 4.f;

    draw_background(time_sec, bass);
    draw_bars(spectrum, time_sec);
    draw_ring(testapp::clamp(position_norm, 0.f, 1.f));
    draw_orb(bass, time_sec, is_playing);
}

} // namespace testapp
