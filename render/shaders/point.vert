#version 440
// Puncte rotunde cu diametru constant în pixeli (o instanță = un punct).
layout(location = 0) in vec2 corner;      // −1 / +1 pe ambele axe
layout(location = 1) in vec3 position;
layout(location = 2) in vec4 color;
layout(location = 3) in float sizePx;     // diametru în px logici

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec3 v_local;    // (poziția în quad în px, raza în px)

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
    vec4 c = frame.viewProj * vec4(position, 1.0);
    float radius = sizePx * frame.viewport.z * 0.5;
    float extent = radius + 1.0;
    vec2 offset = corner * extent / frame.viewport.xy * 2.0;
    gl_Position = vec4(c.xy + offset * c.w, c.z, c.w);
    v_color = color;
    v_local = vec3(corner * extent, radius);
}
