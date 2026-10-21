// Fixed shader screen size
//
// Replaces love_ScreenSize in the shaders with a 1920x1080 constant in high
// precision, for GPUs where it breaks in medium precision.

//! @patch resources/shaders/CRT.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/CRT.fs 76-77
        crt_tex.r = crt_tex.r*(1.-crt_amout_adjusted) + crt_amout_adjusted*Texel( tex, tc + vec2(0.0005*(1. +10.*(artifact_amplifier - 1.))*1600./ScreenSize.x, 0.)).r;
        crt_tex.g = crt_tex.g*(1.-crt_amout_adjusted) + crt_amout_adjusted*Texel( tex, tc + vec2(-0.0005*(1. +10.*(artifact_amplifier - 1.))*1600./ScreenSize.x, 0.)).g;
//! @endpatch

//! @patch resources/shaders/CRT.fs 147-147
    MY_HIGHP_OR_MEDIUMP float mid_dist = screen_scale*length(vertex_position.xy/screen_scale - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/background.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/background.fs 21-22
    MY_HIGHP_OR_MEDIUMP number pixel_size = length(ScreenSize.xy)/PIXEL_SIZE_FAC;
    MY_HIGHP_OR_MEDIUMP vec2 uv = (floor(screen_coords.xy*(1./pixel_size))*pixel_size - 0.5*ScreenSize.xy)/length(ScreenSize.xy) - vec2(0.12, 0.);
//! @endpatch

//! @patch resources/shaders/background.fs 28-28
    MY_HIGHP_OR_MEDIUMP vec2 mid = (ScreenSize.xy/length(ScreenSize.xy))/2.;
//! @endpatch

//! @patch resources/shaders/booster.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/booster.fs 91-91
    MY_HIGHP_OR_MEDIUMP float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/debuff.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/debuff.fs 142-142
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/dissolve.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/dissolve.fs 79-79
    MY_HIGHP_OR_MEDIUMP float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/flash.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/flash.fs 16-17
    number pixel_size = length(ScreenSize.xy)/PIXEL_SIZE_FAC;
    vec2 uv = (floor(screen_coords.xy*(1./pixel_size))*pixel_size - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/foil.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/foil.fs 136-136
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/holo.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/holo.fs 145-145
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/hologram.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/hologram.fs 122-122
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/negative.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/negative.fs 126-126
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/negative_shine.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/negative_shine.fs 91-91
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/played.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/played.fs 123-123
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/polychrome.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/polychrome.fs 143-143
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/skew.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/skew.fs 11-11
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/splash.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/splash.fs 20-21
    MY_HIGHP_OR_MEDIUMP number pixel_size = length(ScreenSize.xy)/PIXEL_SIZE_FAC;
    MY_HIGHP_OR_MEDIUMP vec2 uv = (floor(screen_coords.xy*(1./pixel_size))*pixel_size - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/splash.fs 27-27
    MY_HIGHP_OR_MEDIUMP vec2 mid = (ScreenSize.xy/length(ScreenSize.xy))/2.;
//! @endpatch

//! @patch resources/shaders/vortex.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/vortex.fs 6-6
    vec2 uv = (vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/vortex.fs 11-11
    float len = length(uv * vec2(ScreenSize.x / ScreenSize.y, 1.));
//! @endpatch

//! @patch resources/shaders/vortex.fs 15-15
    vec2 center = 0.5*ScreenSize.xy/length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/vortex.fs 17-18
    vertex_position.x = (radius * cos(angle) + center.x)*length(ScreenSize.xy);
    vertex_position.y = (radius * sin(angle) + center.y)*length(ScreenSize.xy);
//! @endpatch

//! @patch resources/shaders/voucher.fs 1
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define DOPO_SCREEN_PRECISION highp
#else
	#define DOPO_SCREEN_PRECISION mediump
#endif
DOPO_SCREEN_PRECISION vec2 ScreenSize = vec2(1920.0, 1080.0);

//! @endpatch

//! @patch resources/shaders/voucher.fs 91-91
    float mid_dist = length(vertex_position.xy - 0.5*ScreenSize.xy)/length(ScreenSize.xy);
//! @endpatch
