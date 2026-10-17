#version 100
// A wall or sky column, a sprite post (gl.c): texture coordinates and the
// colour table row straight from the attributes; positions in frame
// pixels, row 0 at the top.
attribute vec2 a_pos;
attribute vec2 a_st;
attribute float a_l;
uniform vec2 u_scale;
uniform vec2 u_offset;
varying vec2 v_uv;
varying float v_light;
void main() {
  vec2 p = (a_pos + u_offset) * u_scale;
  v_uv = a_st;
  v_light = a_l;
  gl_Position = vec4(p.x - 1.0, 1.0 - p.y, 0.0, 1.0);
}
