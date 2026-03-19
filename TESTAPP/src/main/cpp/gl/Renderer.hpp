#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  gl/Renderer.hpp
//  OpenGL ES 2.0 visualizer — radial spectrum analyzer
//
//  What it draws (back-to-front):
//    1. Background — dark radial gradient + subtle hex grid
//    2. Spectrum bars — 32 bars radiating from center, height = amplitude
//    3. Center orb — pulsing circle driven by bass energy
//    4. Track ring — thin static ring indicating track progress
// ─────────────────────────────────────────────────────────────────────────────
#include "common.hpp"
#include <GLES2/gl2.h>
#include <array>
#include <string_view>

namespace testapp {

inline constexpr int kVisBands = 32;

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Call from GLSurfaceView.Renderer.onSurfaceCreated
    bool on_surface_created();
    // Call from GLSurfaceView.Renderer.onSurfaceChanged
    void on_surface_changed(int width, int height);
    // Call from GLSurfaceView.Renderer.onDrawFrame
    void draw_frame(const std::array<float, kVisBands>& spectrum,
                    float position_norm,   // 0–1 progress
                    bool  is_playing,
                    float time_sec);

private:
    // ── GL program handles ────────────────────────────────────────────────────
    GLuint prog_bg_    = 0;   // full-screen background
    GLuint prog_bars_  = 0;   // spectrum bars
    GLuint prog_orb_   = 0;   // center orb
    GLuint prog_ring_  = 0;   // progress ring

    // ── VBOs ──────────────────────────────────────────────────────────────────
    GLuint vbo_bg_     = 0;   // 6 verts (2 triangles)
    GLuint vbo_bars_   = 0;   // 32 bars × 6 verts × 3 floats (x,y,amp) — DYNAMIC
    GLuint vbo_orb_    = 0;   // circle fan
    GLuint vbo_ring_   = 0;   // ring segments

    // ── Viewport ──────────────────────────────────────────────────────────────
    int   width_        = 1;
    int   height_       = 1;
    float inv_aspect_   = 1.0f;  // H/W — applied in vertex shader to make circles circular

    // ── Helpers ───────────────────────────────────────────────────────────────
    static GLuint compile_shader(GLenum type, std::string_view src);
    static GLuint link_program(GLuint vert, GLuint frag);
    static GLuint build_program(std::string_view vert_src, std::string_view frag_src);

    void build_bg_geometry();
    void build_orb_geometry(int segments);
    void build_ring_geometry(int segments);
    void update_bars_vbo(const std::array<float, kVisBands>& spectrum);
    void draw_background(float time_sec, float bass);
    void draw_bars(const std::array<float, kVisBands>& spectrum, float time_sec);
    void draw_orb(float bass, float time_sec, bool is_playing);
    void draw_ring(float position_norm);
};

} // namespace testapp
