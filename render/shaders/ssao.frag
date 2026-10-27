#version 440
// SSAO, a doua trecere: cât din emisfera din jurul normalei e acoperită de geometrie apropiată (1 = deloc).
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

layout(binding = 1) uniform sampler2D gbuffer;

// Poziția în spațiul camerei a pixelului cu coordonatele NDC date și adâncimea liniară dată.
vec3 viewPosition(vec2 ndc, float depth)
{
    if (frame.forward.w > 0.5) {
        vec4 p = frame.invProjection * vec4(ndc, 0.0, 1.0);
        return vec3(p.xy / p.w, -depth);
    }
    vec4 p = frame.invProjection * vec4(ndc, 1.0, 1.0);
    vec3 dir = p.xyz / p.w;
    return dir * (depth / -dir.z);
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 size = frame.viewport.xy;
    vec2 uv = gl_FragCoord.xy / size;
    vec4 g = texture(gbuffer, uv);
    if (g.a <= 0.0) {
        fragColor = vec4(1.0);
        return;
    }
    // NDC-ul pixelului (y după convenția backend-ului).
    vec2 ndc = uv * 2.0 - 1.0;
    if ((frame.params.y > 0.5) != (frame.params.x > 0.5))
        ndc.y = -ndc.y;
    vec3 p = viewPosition(ndc, g.a);
    vec3 n = normalize(g.xyz);
    // Raza de căutare: o fracțiune din distanța până la țintă (lucrează la orice scară a modelului).
    float radius = clamp(frame.eye.w * 0.04, 10.0, 2000.0);
    // O bază în jurul normalei, rotită aleator pe pixel (zgomotul se netezește la citire).
    float angle = hash(gl_FragCoord.xy) * 6.2831853;
    vec3 t = normalize(abs(n.z) < 0.9 ? cross(n, vec3(0.0, 0.0, 1.0)) : cross(n, vec3(1.0, 0.0, 0.0)));
    vec3 b = cross(n, t);
    vec3 tr = t * cos(angle) + b * sin(angle);
    vec3 br = cross(n, tr);
    float occlusion = 0.0;
    const int kSamples = 12;
    for (int i = 0; i < kSamples; ++i) {
        // Puncte pe o spirală în emisferă, mai dese aproape de pixel.
        float fi = float(i) + 0.5;
        float r = fi / float(kSamples);
        float phi = fi * 2.399963;
        float z = sqrt(1.0 - r);
        vec3 h = vec3(cos(phi) * sqrt(r), sin(phi) * sqrt(r), z);
        vec3 s = p + (tr * h.x + br * h.y + n * h.z) * radius * mix(0.1, 1.0, r * r);
        vec4 clip = frame.projection * vec4(s, 1.0);
        vec2 sndc = clip.xy / clip.w;
        if ((frame.params.y > 0.5) != (frame.params.x > 0.5))
            sndc.y = -sndc.y;
        vec2 suv = sndc * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0)
            continue;
        float sceneDepth = texture(gbuffer, suv).a;
        if (sceneDepth <= 0.0)
            continue;
        // Acoperit dacă geometria e mai aproape decât punctul de probă; doar în raza căutării.
        float range = smoothstep(0.0, 1.0, radius / abs(-p.z - sceneDepth + 1e-3));
        occlusion += (sceneDepth < -s.z - radius * 0.03 ? 1.0 : 0.0) * range;
    }
    float ao = 1.0 - occlusion / float(kSamples);
    fragColor = vec4(vec3(pow(clamp(ao, 0.0, 1.0), 1.5)), 1.0);
}
