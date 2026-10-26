#version 440
// Umbrele pe sol: planul Z = 0, invizibil, întunecat acolo unde soarele e acoperit.
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
} frame;

layout(binding = 1) uniform sampler2D shadowMap;

vec3 unproject(vec2 ndc, float z)
{
    vec4 p = frame.invViewProj * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}

void main()
{
    float nearZ = frame.viewport.w > 0.5 ? 0.0 : -1.0;
    vec3 a = unproject(v_ndc, nearZ);
    vec3 b = unproject(v_ndc, 1.0);
    float denom = b.z - a.z;
    if (abs(denom) < 1e-12)
        discard;
    float t = -a.z / denom;
    if (t < 0.0 || t > 1.0)
        discard;
    vec3 p = a + (b - a) * t;

    vec4 lp = frame.lightViewProj * vec4(p, 1.0);
    vec3 ndc = lp.xyz / lp.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    if (frame.params.y < 0.5)
        uv.y = 1.0 - uv.y;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0)
        discard;
    float depth = frame.viewport.w > 0.5 ? ndc.z : ndc.z * 0.5 + 0.5;
    float shade = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float stored = texture(shadowMap, uv + vec2(x, y) * frame.shadow.z * 1.5).r;
            shade += depth - 0.002 > stored ? 1.0 : 0.0;
        }
    }
    shade /= 25.0;
    if (shade <= 0.001)
        discard;
    float dist = max(frame.eye.w, 1.0);
    float horizontal = length(p.xy - frame.eye.xy);
    float fade = 1.0 - smoothstep(dist * 3.0, dist * 10.0, horizontal);

    // Ca grid-ul: adâncimea împinsă puțin în spate, ca fețele de pe sol să acopere umbra.
    vec3 away = frame.forward.w > 0.5 ? frame.forward.xyz : normalize(p - frame.eye.xyz);
    vec4 clip = frame.viewProj * vec4(p + away * dist * 0.002, 1.0);
    float d = clip.z / clip.w;
    gl_FragDepth = frame.viewport.w > 0.5 ? d : d * 0.5 + 0.5;
    fragColor = vec4(0.0, 0.0, 0.0, shade * frame.shadow.w * 0.45 * fade);
}
