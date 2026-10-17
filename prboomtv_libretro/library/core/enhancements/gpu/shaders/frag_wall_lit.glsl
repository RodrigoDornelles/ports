#version 100
// frag_wall plus the dynamic lights (prboom-dynlight, gl.c): each adds its
// colour, fading to nothing at its radius and with the angle it meets the
// wall at, to the texel at full brightness, so the light shows even where
// the sector is dark.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform vec4 u_lpos[8];
uniform vec3 u_lcol[8];
uniform int u_lcount;
uniform float u_full;
varying vec2 v_uv;
varying float v_light;
varying vec3 v_world;
varying vec3 v_normal;
void main() {
  float index = texture2D(u_tex, v_uv).r;
  float u = index * (255.0 / 256.0) + (0.5 / 256.0);
  vec3 base = texture2D(u_cmap, vec2(u, v_light)).rgb;
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
