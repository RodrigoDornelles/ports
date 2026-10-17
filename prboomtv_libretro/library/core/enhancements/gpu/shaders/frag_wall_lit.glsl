#version 100
// frag_wall plus the dynamic lights (prboom-dynlight, gl.c): each adds its
// colour, fading to nothing at its radius and with the angle it meets the
// wall at, to the texel at full brightness, so the light shows even where
// the sector is dark; a brightmap as in frag_wall_bm.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform vec4 u_lpos[8];
uniform vec3 u_lcol[8];
uniform int u_lcount;
uniform float u_full;
uniform vec2 u_bm;
varying vec2 v_uv;
varying float v_light;
varying vec3 v_world;
varying vec3 v_normal;
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
  vec4 t = texture2D(u_tex, v_uv);
  float u = t.r * (255.0 / 256.0) + (0.5 / 256.0);
  vec3 base = texture2D(u_cmap, vec2(u, bm_row(t.a, v_light))).rgb;
  vec3 full = texture2D(u_cmap, vec2(u, u_full)).rgb;
  vec3 sum = vec3(0.0);
  for (int i = 0; i < 8; i++) {
    if (i >= u_lcount)
      break;
    vec3 d = u_lpos[i].xyz - v_world;
    float dist = length(d);
    float att = clamp(1.0 - dist / u_lpos[i].w, 0.0, 1.0);
    float ndl = clamp(dot(d, v_normal) / max(dist, 0.001), 0.0, 1.0);
    sum += u_lcol[i] * (att * ndl);
  }
  gl_FragColor = vec4(base + full * sum, 1.0);
}
