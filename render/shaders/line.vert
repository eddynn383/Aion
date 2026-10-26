#version 440
// Segmente cu grosime constantă în pixeli: fiecare instanță e un segment,
// extins pe ecran într-un quad (6 vârfuri) cu margine pentru anti-aliasing.
layout(location = 0) in vec2 corner;      // x: 0 = capătul a, 1 = capătul b; y: −1 / +1 = partea
layout(location = 1) in vec3 pointA;
layout(location = 2) in vec3 pointB;
layout(location = 3) in vec4 color;
layout(location = 4) in vec2 style;       // x = grosime (px logici), y = lungimea liniuței (px logici; 0 = continuă)

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec3 v_side;     // (distanța de la axă în px · w, w, jumătatea grosimii)
layout(location = 2) out vec3 v_along;    // (distanța de-a lungul segmentului în px · w, w, lungimea liniuței)

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
    vec4 params;       // x = 1 dacă y crește în sus în NDC
} frame;

// Liniile se apropie puțin de cameră, ca muchiile să rămână deasupra fețelor pe care stau.
vec3 towardCamera(vec3 p)
{
    const float bias = 0.002;
    if (frame.forward.w > 0.5)
        return p - frame.forward.xyz * frame.eye.w * bias;
    return mix(p, frame.eye.xyz, bias);
}

void main()
{
    vec4 ca = frame.viewProj * vec4(towardCamera(pointA), 1.0);
    vec4 cb = frame.viewProj * vec4(towardCamera(pointB), 1.0);

    // Tăiem segmentul la planul apropiat, ca o axă care trece prin spatele camerei să rămână corectă.
    const float minW = 1e-4;
    if (ca.w < minW && cb.w < minW) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
    if (ca.w < minW)
        ca = mix(ca, cb, (minW - ca.w) / (cb.w - ca.w));
    if (cb.w < minW)
        cb = mix(cb, ca, (minW - cb.w) / (ca.w - cb.w));

    vec2 size = frame.viewport.xy;
    float dpr = frame.viewport.z;
    vec2 sa = (ca.xy / ca.w * 0.5 + 0.5) * size;
    vec2 sb = (cb.xy / cb.w * 0.5 + 0.5) * size;
    vec2 d = sb - sa;
    float len = length(d);
    vec2 dir = len > 1e-4 ? d / len : vec2(1.0, 0.0);
    vec2 normal = vec2(-dir.y, dir.x);

    float halfWidth = style.x * dpr * 0.5;
    float extent = halfWidth + 1.0; // un pixel în plus pentru anti-aliasing
    vec4 c = corner.x < 0.5 ? ca : cb;
    vec2 s = (corner.x < 0.5 ? sa : sb) + normal * corner.y * extent + dir * (corner.x < 0.5 ? -1.0 : 1.0) * 1.0;

    vec2 ndc = s / size * 2.0 - 1.0;
    gl_Position = vec4(ndc * c.w, c.z, c.w);

    float along = (corner.x < 0.5 ? -1.0 : len + 1.0);
    v_color = color;
    v_side = vec3(corner.y * extent * c.w, c.w, halfWidth);
    v_along = vec3(along * c.w, c.w, style.y * dpr);
}
