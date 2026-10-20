#version 100
// A corona (prboom-glow 2, gl.c): a soft halo, the radial ramp in the
// texture times the glowing sprite's colour, added to what is behind.
precision mediump float;
uniform sampler2D u_tex;
uniform vec4 u_color;
varying vec2 v_uv;
varying float v_light;
void main() {
  gl_FragColor = texture2D(u_tex, v_uv).r * u_color;
}
