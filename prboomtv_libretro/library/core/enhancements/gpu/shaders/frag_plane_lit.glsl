#version 100
// frag_plane plus the dynamic lights (prboom-dynlight, gl.c), as
// frag_wall_lit adds them.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform sampler2D u_zlight;
uniform float u_rows;
uniform vec4 u_lpos[8];
uniform vec3 u_lcol[8];
uniform int u_lcount;
uniform float u_full;
varying vec2 v_st;
varying float v_z;
varying float v_lev;
varying float v_base;
varying vec3 v_world;
varying vec3 v_normal;
void main() {
  float index = texture2D(u_tex, v_st).r;
  float r = floor(texture2D(u_zlight, vec2(v_z, v_lev)).r * 255.0 + 0.5);
  float u = index * (255.0 / 256.0) + (0.5 / 256.0);
  vec3 base = texture2D(u_cmap, vec2(u, (r + v_base + 0.5) / u_rows)).rgb;
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
