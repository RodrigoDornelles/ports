// Solid background
//
// The menu and table backgrounds become a solid colour instead of the animated
// paint shaders. Card shaders are untouched.

//! @patch resources/shaders/background.fs 1-52
// DOPO_SOLID_BACKGROUND
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define MY_HIGHP_OR_MEDIUMP highp
#else
	#define MY_HIGHP_OR_MEDIUMP mediump
#endif

extern MY_HIGHP_OR_MEDIUMP number time;
extern MY_HIGHP_OR_MEDIUMP number spin_time;
extern MY_HIGHP_OR_MEDIUMP vec4 colour_1;
extern MY_HIGHP_OR_MEDIUMP vec4 colour_2;
extern MY_HIGHP_OR_MEDIUMP vec4 colour_3;
extern MY_HIGHP_OR_MEDIUMP number contrast;
extern MY_HIGHP_OR_MEDIUMP number spin_amount;

vec4 effect( vec4 colour, Image texture, vec2 texture_coords, vec2 screen_coords )
{
    MY_HIGHP_OR_MEDIUMP number probe = time + spin_time + contrast + spin_amount;
    if (probe < -9999.0) {
        return colour_2 + colour_3;
    }
    return vec4(colour_1.rgb, 1.0);
}
//! @endpatch

//! @patch resources/shaders/splash.fs 1-56
// DOPO_SOLID_BACKGROUND
#if defined(VERTEX) || __VERSION__ > 100 || defined(GL_FRAGMENT_PRECISION_HIGH)
	#define MY_HIGHP_OR_MEDIUMP highp
#else
	#define MY_HIGHP_OR_MEDIUMP mediump
#endif

extern MY_HIGHP_OR_MEDIUMP number time;
extern MY_HIGHP_OR_MEDIUMP number vort_speed;
extern MY_HIGHP_OR_MEDIUMP vec4 colour_1;
extern MY_HIGHP_OR_MEDIUMP vec4 colour_2;
extern MY_HIGHP_OR_MEDIUMP number mid_flash;
extern MY_HIGHP_OR_MEDIUMP number vort_offset;

vec4 effect( vec4 colour, Image texture, vec2 texture_coords, vec2 screen_coords )
{
    MY_HIGHP_OR_MEDIUMP number probe = time + vort_speed + vort_offset;
    if (probe < -9999.0) {
        return colour_2;
    }
    MY_HIGHP_OR_MEDIUMP vec4 base = mix(colour_1, vec4(1.0), min(max(mid_flash, 0.0), 1.0));
    return vec4(base.rgb, 1.0);
}
//! @endpatch
