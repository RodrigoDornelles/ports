#version 100
// Texel (palette index) -> colour table at the column's light row. No sum
// on v_uv before the first texture2D: the Mali-400 has no highp in the
// fragment shader, and a coordinate that goes straight from the varying
// keeps its precision.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
varying vec2 v_uv;
varying float v_light;
void main() {
  float index = texture2D(u_tex, v_uv).r;
  gl_FragColor = texture2D(u_cmap, vec2(index * (255.0 / 256.0) + (0.5 / 256.0), v_light));
}
