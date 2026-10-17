#version 100
// A patch of the 2D (status bar, HUD, menus, text, gl.c): texel index and
// coverage from the patch, the colour translation (red, gold... text, a
// player's colours) as a row of a table, then the palette as the frame
// has it.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_trans;
uniform sampler2D u_pal;
varying vec2 v_uv;
varying float v_light;
void main() {
  vec4 t = texture2D(u_tex, v_uv);
  if (t.a < 0.5)
    discard;
  float index = texture2D(u_trans, vec2(t.r * (255.0 / 256.0) + (0.5 / 256.0), v_light)).r;
  gl_FragColor = texture2D(u_pal, vec2(index * (255.0 / 256.0) + (0.5 / 256.0), 0.5));
}
