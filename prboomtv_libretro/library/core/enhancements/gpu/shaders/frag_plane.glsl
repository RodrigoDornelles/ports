#version 100
// The light per pixel: the interpolated distance picks the distance band
// in the light table (zlight: rows 0-15 light levels, 16 always row 0, 17
// always row 32), which gives the COLORMAP row for the colour table.
precision mediump float;
uniform sampler2D u_tex;
uniform sampler2D u_cmap;
uniform sampler2D u_zlight;
uniform float u_rows;
varying vec2 v_st;
varying float v_z;
varying float v_lev;
varying float v_base;
void main() {
  float index = texture2D(u_tex, v_st).r;
  float r = floor(texture2D(u_zlight, vec2(v_z, v_lev)).r * 255.0 + 0.5);
  gl_FragColor = texture2D(u_cmap, vec2(index * (255.0 / 256.0) + (0.5 / 256.0),
                                        (r + v_base + 0.5) / u_rows));
}
