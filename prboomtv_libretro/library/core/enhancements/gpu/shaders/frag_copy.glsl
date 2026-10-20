#version 100
// An image the CPU made, already in colour (a cached full-screen page, the
// status bar background, gl.c), copied as it is.
precision mediump float;
uniform sampler2D u_tex;
varying vec2 v_uv;
varying float v_light;
void main() {
  gl_FragColor = texture2D(u_tex, v_uv);
}
