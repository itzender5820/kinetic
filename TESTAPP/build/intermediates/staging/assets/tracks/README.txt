TESTAPP — Audio Tracks
══════════════════════════════════════════
All audio is synthesized in real-time by the C++20 AudioEngine.
No audio files are required in this directory.

The engine generates 4 tracks using additive synthesis (OpenSL ES):
  1. Alpha Wave    — 432 Hz fundamental + harmonics
  2. Solar Wind    — 528 Hz fundamental + harmonics
  3. Deep Space    — 396 Hz fundamental + harmonics
  4. Cosmic Ray    — 639 Hz fundamental + harmonics

Each track plays for 60 seconds with fade-in/fade-out envelopes.
Spectrum analysis (Goertzel algorithm, 32 bands) drives the OpenGL visualizer.
