#version 100
// A floor or ceiling rectangle a dynamic light reaches (prboom-dynlight,
// gl.c): as vert_plane, plus where the corner is from the eye (in map units / 64) and the
// plane's normal, interpolated in perspective like the rest.
attribute vec2 a_pos;
attribute vec2 a_st;
attribute float a_w;
attribute float a_z;
attribute vec2 a_lb;
attribute vec3 a_world;
attribute vec3 a_normal;
uniform vec2 u_scale;
uniform vec2 u_offset;
varying vec2 v_st;
varying float v_z;
varying float v_lev;
varying float v_base;
varying vec3 v_world;
varying vec3 v_normal;
void main() {
  vec2 p = (a_pos + u_offset) * u_scale;
  v_st = a_st;
  v_z = a_z;
  v_lev = (a_lb.x + 0.5) / 32.0;
  v_base = a_lb.y;
  v_world = a_world;
  v_normal = a_normal;
  gl_Position = vec4((p.x - 1.0) * a_w, (1.0 - p.y) * a_w, 0.0, a_w);
}
