#version 100
// frag_plane for a flat with a brightmap (gl.c): a texel takes at least
// its brightmap's level of light.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform sampler2D u_zlight;
uniform float u_rows;
uniform vec2 u_bm;
varying vec2 v_st;
varying float v_z;
varying float v_lev;
varying float v_base;
// The row a texel takes with a brightmap (gl.c): its alpha is 255 when
// it has none, else 1..127, how bright it is at least; it takes the
// lighter of its light's row and that level's among the 32 light rows of
// the full bright COLORMAP (u_bm: that row, the colour table's height).
float bm_row(float a, float row) {
  float lv = a * 255.0;
  if (lv > 127.5)
    return row;
  float r = row * u_bm.y - 0.5;
  float rb = u_bm.x + floor((1.0 - lv / 127.0) * 31.0 + 0.5);
  return (min(r, rb) + 0.5) / u_bm.y;
}
void main() {
  vec4 t = texture2D(u_tex, v_st);
  float r = floor(texture2D(u_zlight, vec2(v_z, v_lev)).r * 255.0 + 0.5);
  float row = bm_row(t.a, (r + v_base + 0.5) / u_rows);
  gl_FragColor = texture2D(u_cmap, vec2(t.r * (255.0 / 256.0) + (0.5 / 256.0), row));
}
