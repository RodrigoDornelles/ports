// WebGL
//
// What Balatro needs to run on WebGL, always applied when it plays in the
// browser: WebGL only loops over constant bounds.

//! @patch resources/shaders/hologram.fs 58-58
    const int glow_samples = 4;
//! @endpatch
