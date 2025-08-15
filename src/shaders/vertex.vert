#version 450

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PC {
    float time;
    float dt;
    uint  count;
} pc;

layout(location = 0) in  vec3 inPos;
layout(location = 7) in  vec4 ampFreqPhaseOff;
layout(location = 8) in  vec4 centerType;

layout(location = 0) out vec3 color;

/* ---------- movement helpers ---------- */
vec3 petal(vec3 c, float a, float f, float p, float t) {
    float r = a * (0.5 + 0.5 * sin(t * f * 0.3));
    return c + vec3(r * cos(t * f + p), r * sin(t * f + p), 0);
}

vec3 orbit(vec3 c, float a, float f, float p, float t) {
    float ang = t * f + p;
    return c + vec3(a * cos(ang), a * sin(ang), 0);
}

vec3 wave(vec3 c, float a, float f, float p, float t) {
    return c + vec3(a * sin(t * f + p), a * cos(t * f * 0.5 + p), 0);
}

vec3 triangle(vec3 c, float a, float f, float p, float t) {
    float ang = t * f + p;
    float w   = a * 2.0 * (1.0 + 0.3 * sin(ang * 1.5));
    return c + vec3(w * cos(ang), w * sin(ang), 0);
}

/* ---------- HSV → RGB ---------- */
vec3 hsv2rgb(float h, float s, float v) {
    float c  = v * s;
    float x  = c * (1.0 - abs(mod(h * 6.0, 2.0) - 1.0));
    float m  = v - c;
    vec3  rgb;

    if      (h < 1.0/6.0) rgb = vec3(c, x, 0);
    else if (h < 2.0/6.0) rgb = vec3(x, c, 0);
    else if (h < 3.0/6.0) rgb = vec3(0, c, x);
    else if (h < 4.0/6.0) rgb = vec3(0, x, c);
    else if (h < 5.0/6.0) rgb = vec3(x, 0, c);
    else                    rgb = vec3(c, 0, x);

    return rgb + m;
}

void main() {
    float t  = pc.time + ampFreqPhaseOff.w;
    vec3  c  = centerType.xyz;
    float a  = ampFreqPhaseOff.x;
    float f  = ampFreqPhaseOff.y;
    float p  = ampFreqPhaseOff.z;
    uint  type = uint(centerType.w);

    /* pick movement pattern */
    vec3 pos = (type == 0) ? petal(c,a,f,p,t) :
               (type == 1) ? orbit(c,a,f,p,t) :
               (type == 2) ? wave(c,a,f,p,t)  :
                             triangle(c,a,f,p,t);

    /* colour */
    float hue = mod(p + t * 0.5, 6.28318) / 6.28318;
    color = hsv2rgb(hue, 0.8, 0.9);

    /* transform */
    float rot = t * 0.1;
    mat4 m = mat4(
        cos(rot),  sin(rot), 0, 0,
       -sin(rot),  cos(rot), 0, 0,
        0,         0,        1, 0,
        pos,                   1);

    gl_Position = ubo.proj * ubo.view * m * vec4(inPos, 1.0);
}