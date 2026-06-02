#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 inWorldPos;
layout(location = 0) out vec4 outColor;

vec3 lightPos = vec3(5, 5, 5);

// RGB to HSV conversion
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV to RGB conversion
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    // Get flat normal and calculate standard diffuse factor (0.0 to 1.0)
    vec3 dX = dFdx(inWorldPos);
    vec3 dY = dFdy(inWorldPos);
    vec3 flatNormal = normalize(cross(dX, dY));
    vec3 lightDir = normalize(lightPos - inWorldPos);

    // N.L gives 1.0 in direct light, 0.0 in shadow
    float diffuseFactor = max(dot(flatNormal, lightDir), 0.0);

    // Convert base vertex color to HSV
    vec3 baseHSV = rgb2hsv(fragColor);

    // Define styled shifts
    // Highlight: Shift hue toward yellow, boost saturation, lower value slightly
    float hueYellow = 0.02;
    vec3 lightHSV = baseHSV + vec3(hueYellow, 0.15, -0.10); 

    // Shadow: Shift hue toward blue, lower saturation, boost value/brightness
    float hueBlue = -0.04;
    vec3 shadowHSV = baseHSV + vec3(hueBlue, -0.20, 0.15);

    // Clamp values so they don't break the 0.0-1.0 HSV boundaries
    lightHSV = clamp(lightHSV, 0.0, 1.0);
    shadowHSV = clamp(shadowHSV, 0.0, 1.0);

    // Linearly interpolate between shadow and light based on the lighting factor
    vec3 finalHSV = mix(shadowHSV, lightHSV, diffuseFactor);

    // Convert back to RGB for output
    outColor = vec4(hsv2rgb(finalHSV), 1.0);
}
