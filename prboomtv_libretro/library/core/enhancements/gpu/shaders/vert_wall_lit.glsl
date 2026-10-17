#version 100
// A wall column a dynamic light reaches (prboom-dynlight, gl.c): as
// vert_wall, plus where the corner is from the eye and the wall's normal.
attribute vec2 a_pos;
attribute vec2 a_st;
attribute float a_l;
attribute vec3 a_world;
attribute vec3 a_normal;
uniform vec2 u_scale;
uniform vec2 u_offset;
varying vec2 v_uv;
varying float v_light;
varying vec3 v_world;
varying vec3 v_normal;
void main() {
  vec2 p = (a_pos + u_offset) * u_scale;
  v_uv = a_st;
  v_light = a_l;
  v_world = a_world;
  v_normal = a_normal;
  gl_Position = vec4(p.x - 1.0, 1.0 - p.y, 0.0, 1.0);
}
