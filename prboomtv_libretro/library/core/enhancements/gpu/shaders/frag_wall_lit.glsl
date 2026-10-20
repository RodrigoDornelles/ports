#version 100
// frag_wall plus the dynamic lights (prboom-dynlight, gl.c): each adds its
// colour, fading to nothing at its radius and with the angle it meets the
// wall at, to the texel at full brightness, so the light shows even where
// the sector is dark; a brightmap as in frag_wall_bm.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
// LIGHTS: defined by gl.c, as many as the GPU takes (8, 16 or 32)
uniform vec4 u_lpos[LIGHTS];
uniform vec4 u_lcol[LIGHTS];   // colour, and 1 when it fades with the angle
uniform int u_lcount;
#ifdef SHADOWS
uniform sampler2D u_shadow; // per light: the nearest wall per direction (gl.c)
#endif
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
  for (int i = 0; i < LIGHTS; i++) {
    if (i >= u_lcount)
      break;
#ifdef SHADOWS
    // in the light's shadow: past the nearest wall that way (prboom-shadowmap)
    vec2 sd = v_world.xy - u_lpos[i].xy;
    float sa = atan(sd.y, sd.x) * 0.15915494;
    vec4 sw = texture2D(u_shadow, vec2(sa < 0.0 ? sa + 1.0 : sa, (float(i) + 0.5) / float(LIGHTS)));
    if (length(sd) > (sw.r * (65280.0 / 65535.0) + sw.a * (255.0 / 65535.0)) * u_lpos[i].w + 0.0625)
      continue;
#endif
    vec3 d = u_lpos[i].xyz - v_world;
    float dist = length(d);
    float att = clamp(1.0 - dist / u_lpos[i].w, 0.0, 1.0);
    float ndl = mix(1.0, clamp(dot(d, v_normal) / max(dist, 0.001), 0.0, 1.0), u_lcol[i].w);
    sum += u_lcol[i].rgb * (att * ndl);
  }
  gl_FragColor = vec4(base + full * sum, 1.0);
}
