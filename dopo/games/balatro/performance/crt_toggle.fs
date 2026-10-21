// CRT only when on
//
// Skips the CRT shader while its intensity is zero, and gives the shader a
// cheap path when all of its effects are off.

//! @patch resources/shaders/CRT.fs 21-21
{ if (crt_intensity <= 0.000001 && noise_fac <= 0.000001 && glitch_intensity <= 0.000001) { MY_HIGHP_OR_MEDIUMP vec2 ftc = (tc*2.0 - vec2(1.0))*scale_fac; ftc += (ftc.yx*ftc.yx)*ftc*(distortion_fac - 1.0); MY_HIGHP_OR_MEDIUMP number fmask = (1.0 - smoothstep(1.0-feather_fac,1.0,abs(ftc.x) - BUFF))*(1.0 - smoothstep(1.0-feather_fac,1.0,abs(ftc.y) - BUFF)); ftc = (ftc + vec2(1.0))/2.0; MY_HIGHP_OR_MEDIUMP vec4 fcol = Texel(tex, ftc); fcol.rgb = (fcol.rgb - vec3(0.55))*1.14 + vec3(0.5); fcol.a = 1.0; return fcol*fmask; }
//! @endpatch
