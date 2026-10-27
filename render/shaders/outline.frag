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
    vec4 lightDir;     // xyz = direcția spre soare (normalizată), w = intensitatea luminii directe
    vec4 faceFront;
    vec4 faceBack;
    vec4 forward;      // xyz = direcția de privire, w = 1 pentru proiecție ortografică
    vec4 accent;       // culoarea selecției
    vec4 params;       // x = 1 dacă y crește în sus în NDC, y = 1 dacă y crește în sus în framebuffer
    mat4 lightViewProj; // pentru umbre: spațiul hărții de umbre
    vec4 shadow;       // x = 1 dacă umbrele sunt active, y = bias, z = 1 / mărimea hărții, w = intensitatea umbrei
    vec4 style;        // x = stilul (0 shaded, 1 clay, 2 linii ascunse, 3 wireframe, 4 x-ray), y = ceață, z/w = începutul/sfârșitul ceții
    vec4 section;      // planul de secțiune activ: xyz = normala, w = d (n·x = d); normală nulă = fără secțiune
    vec4 lightCount;   // x = numărul de lumini ale modelului (cel mult 8)
    vec4 lightPos[8];  // xyz = poziția, w = tipul (0 punctuală, 1 spot, 2 direcțională, 3 suprafață)
    vec4 lightColor[8]; // rgb = culoarea liniară × intensitatea, w = raza
    vec4 lightDirection[8]; // xyz = încotro bate lumina, w = cos(jumătatea conului)
    vec4 lightExtra[8]; // x = cos(începutul marginii conului), y = mărimea (suprafață)
    mat4 spotViewProj;  // harta de umbre a spotului
    vec4 spotShadow;    // x = 1 dacă e activă, y = indexul luminii, z = pasul de filtrare, w = bias (mm)
    vec4 spotRange;     // x = planul apropiat, y = cel îndepărtat (mm)
    vec4 envZenith;     // mediul: rgb liniar, a = intensitatea luminii ambientale
    vec4 envHorizon;
    vec4 envGround;
    vec4 envParams;     // x = tăria reflexiilor, y = 1 dacă SSAO e activă
    mat4 view;
    mat4 projection;
    mat4 invProjection;
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
