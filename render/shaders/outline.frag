#version 440
// Conturul luminos al selecției, în screen-space: pixelii din afara măștii, aflați aproape de ea,
// primesc culoarea de accent, cu intensitate descrescătoare spre exterior.
layout(location = 0) in vec2 v_ndc;
layout(location = 0) out vec4 fragColor;

// Trebuie să corespundă cu FrameUniforms din render/src/ViewportRenderer.cpp.
layout(std140, binding = 0) uniform Frame {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 eye;          // xyz = poziția camerei, w = distanța până la țintă
    vec4 viewport;     // xy = mărime în pixeli fizici, z = device pixel ratio, w = 1 dacă adâncimea NDC e în [0, 1]
    vec4 bgTop;
    vec4 bgBottom;
    vec4 gridMinor;
    vec4 gridMajor;
    vec4 lightDir;     // xyz = direcția spre lumină (normalizată)
    vec4 faceFront;
    vec4 faceBack;
    vec4 forward;      // xyz = direcția de privire, w = 1 pentru proiecție ortografică
    vec4 accent;       // culoarea selecției
    vec4 params;       // x = 1 dacă y crește în sus în NDC
} frame;

layout(binding = 1) uniform sampler2D selectionMask;

void main()
{
    vec2 size = frame.viewport.xy;
    vec2 uv = gl_FragCoord.xy / size;
    if (texture(selectionMask, uv).r > 0.5)
        discard;

    float radius = 4.0 * frame.viewport.z;
    float glow = 0.0;
    for (int ring = 1; ring <= 3; ++ring) {
        float r = radius * float(ring) / 3.0;
        float weight = 1.0 - float(ring - 1) / 3.0;
        for (int i = 0; i < 12; ++i) {
            float a = 6.2831853 * (float(i) + 0.5 * float(ring)) / 12.0;
            vec2 offset = vec2(cos(a), sin(a)) * r / size;
            glow = max(glow, texture(selectionMask, uv + offset).r * weight);
        }
    }
    if (glow < 0.01)
        discard;
    fragColor = vec4(mix(frame.accent.rgb, vec3(1.0), 0.15), min(1.0, glow * 1.1));
}
