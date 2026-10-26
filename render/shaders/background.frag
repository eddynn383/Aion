#version 440
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

void main()
{
    float up = frame.params.x > 0.5 ? v_ndc.y : -v_ndc.y;
    float t = clamp(up * 0.5 + 0.5, 0.0, 1.0);
    fragColor = vec4(mix(frame.bgBottom.rgb, frame.bgTop.rgb, t), 1.0);
}
