#version 100
// The software frame, blended ONE, SRC_ALPHA over what the GPU painted: a
// pixel of the frame covers it (alpha 0), the key lets it show (alpha 1),
// the fuzz key darkens it as vanilla's fuzz did (COLORMAP row 6, 26/32:
// the 16-bit fuzz's 15/16 only shows because it compounds from column to
// column, which a single key cannot, and a Spectre must stay visible). A
// key is within half a 565 step.
precision mediump float;
uniform sampler2D u_frame;
uniform vec3 u_keys[2];
varying vec2 v_uv;
float near(vec3 c, vec3 k) {
  vec3 d = abs(c - k);
  return max(max(d.r, d.g), d.b) < 0.004 ? 1.0 : 0.0;
}
void main() {
  vec3 c = texture2D(u_frame, v_uv).rgb;
  float keep = near(c, u_keys[0]) + near(c, u_keys[1]) * 0.8125;
  gl_FragColor = keep > 0.0 ? vec4(0.0, 0.0, 0.0, keep) : vec4(c, 0.0);
}
