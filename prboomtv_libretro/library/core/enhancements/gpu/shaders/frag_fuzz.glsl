#version 100
// A fuzzy thing's post (prboom-spectre): the column's colour scaled and
// offset for the blend of the chosen look (gl_fuzz_state in gl.c); a
// whole column (prboom-scale) cut by the texture's coverage.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform vec4 u_mul;
uniform vec4 u_add;
varying vec2 v_uv;
varying float v_light;
void main() {
  vec4 t = texture2D(u_tex, v_uv);
  if (t.a * 255.0 < 0.5)
    discard;
  vec4 c = texture2D(u_cmap, vec2(t.r * (255.0 / 256.0) + (0.5 / 256.0), v_light));
  gl_FragColor = c * u_mul + u_add;
}
