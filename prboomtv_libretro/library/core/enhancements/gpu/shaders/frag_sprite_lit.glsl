#version 100
// A sprite near dynamic lights (prboom-dynlight, gl.c): their colour,
// summed at the sprite's middle on the CPU, added to its texels at full
// brightness; the coverage cuts the outline as in frag_sprite.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform vec3 u_light;
uniform float u_full;
varying vec2 v_uv;
varying float v_light;
void main() {
  vec4 t = texture2D(u_tex, v_uv);
  if (t.a < 0.5)
    discard;
  float u = t.r * (255.0 / 256.0) + (0.5 / 256.0);
  vec3 base = texture2D(u_cmap, vec2(u, v_light)).rgb;
  vec3 full = texture2D(u_cmap, vec2(u, u_full)).rgb;
  gl_FragColor = vec4(base + full * u_light, 1.0);
}
