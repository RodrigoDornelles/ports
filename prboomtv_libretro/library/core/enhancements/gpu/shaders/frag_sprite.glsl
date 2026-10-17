#version 100
// A sprite's whole column (prboom-scale, gl.c): like frag_wall, the
// texture's coverage cutting the outline.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
varying vec2 v_uv;
varying float v_light;
void main() {
  vec4 t = texture2D(u_tex, v_uv);
  if (t.a < 0.5)
    discard;
  gl_FragColor = texture2D(u_cmap, vec2(t.r * (255.0 / 256.0) + (0.5 / 256.0), v_light));
}
