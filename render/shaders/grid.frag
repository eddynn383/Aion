#version 440
// Grid infinit pe planul Z = 0: intersectăm raza fiecărui pixel cu planul,
// desenăm liniile cu anti-aliasing analitic și le estompăm cu distanța.
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
    vec4 params;       // x = 1 dacă y crește în sus în NDC, z = pasul fix al grid-ului (0 = automat)
} frame;

vec3 unproject(vec2 ndc, float z)
{
    vec4 p = frame.invViewProj * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}

// Acoperirea (0..1) a liniilor de grid cu pasul dat, în jurul punctului p.
float gridLines(vec2 p, float spacing, float widthPx)
{
    vec2 coord = p / spacing;
    vec2 deriv = fwidth(coord);
    vec2 g = abs(fract(coord - 0.5) - 0.5) / max(deriv, vec2(1e-6));
    float line = min(g.x, g.y);
    float coverage = 1.0 - clamp(line - (widthPx * 0.5 - 0.5), 0.0, 1.0);
    // Când liniile ajung mai dese decât câțiva pixeli, se estompează (fără moiré).
    float density = max(deriv.x, deriv.y);
    return coverage * (1.0 - smoothstep(0.15, 0.45, density));
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

    // Pasul se adaptează la distanța camerei: puteri ale lui 10 (mm, cm, dm, m, ...),
    // cu trecere lină între niveluri.
    float dist = max(frame.eye.w, 1.0);
    float level = log(dist * 0.02) / log(10.0);
    float base = pow(10.0, floor(level));
    float blend = fract(level);
    // Pas fix (Snap la grid): liniile mici la pas, cele mari la 10 pași; densitatea le estompează singură.
    if (frame.params.z > 0.0) {
        base = frame.params.z;
        blend = 0.0;
    }

    float minor = gridLines(p.xy, base, 1.0) * (1.0 - blend);
    float major = gridLines(p.xy, base * 10.0, 1.0);

    // Estompare cu distanța față de cameră, pe orizontală.
    float horizontal = length(p.xy - frame.eye.xy);
    float fadeDist = dist * 12.0 + abs(frame.eye.z) * 4.0;
    float fade = 1.0 - smoothstep(fadeDist * 0.25, fadeDist, horizontal);

    float wMinor = frame.gridMinor.a * minor;
    float wMajor = frame.gridMajor.a * major;
    float alpha = max(wMinor, wMajor) * fade;
    if (alpha <= 0.001)
        discard;
    vec3 rgb = mix(frame.gridMinor.rgb, frame.gridMajor.rgb, wMajor / (wMinor + wMajor + 1e-6));

    // Adâncimea grid-ului e împinsă puțin în spatele planului, ca fețele de pe sol (Z = 0) să-l acopere.
    vec3 away = frame.forward.w > 0.5 ? frame.forward.xyz : normalize(p - frame.eye.xyz);
    vec4 clip = frame.viewProj * vec4(p + away * dist * 0.002, 1.0);
    float depth = clip.z / clip.w;
    gl_FragDepth = frame.viewport.w > 0.5 ? depth : depth * 0.5 + 0.5;
    fragColor = vec4(rgb, alpha);
}
