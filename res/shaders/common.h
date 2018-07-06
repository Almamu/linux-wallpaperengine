#define M_PI 3.14159265359
#define M_PI_HALF 1.57079632679
#define M_PI_2 6.28318530718

#define SQRT_2 1.41421356237
#define SQRT_3 1.73205080756

///////////////////////////////////////////
// Color conversion
///////////////////////////////////////////
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 rgb2hsv(vec3 RGB)
{
    vec4 P = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0/3.0) : vec4(RGB.gb, 0.0, -1.0/3.0);
    vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6 * C + 1e-10) + Q.z);

    vec3 HCV = vec3(H, C, Q.x);
    float S = HCV.y / (HCV.z + 1e-10);
    return vec3(HCV.x, S, HCV.z);
}

vec2 rotateVec2(vec2 v, float r)
{
    vec2 cs = vec2(cos(r), sin(r));
    return vec2(v.x * cs.x - v.y * cs.y,
                v.x * cs.y + v.y * cs.x);
}

float greyscale(vec3 color)
{
    return dot(color, vec3(0.11, 0.59, 0.3));
}