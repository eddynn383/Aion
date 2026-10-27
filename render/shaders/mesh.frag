#version 440
// Fețele: material (culoare, rugozitate, metalic), lumină de cer + soare cu umbre moi (PCF),
// tonemapping ACES în spațiu liniar, ceață. Stilurile de afișare schimbă modul de colorare.
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world;
layout(location = 2) in float v_state;
layout(location = 3) in vec4 v_color;
layout(location = 4) in vec2 v_material;
layout(location = 5) in vec4 v_backColor;
layout(location = 6) in vec3 v_uv;

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
} frame;

layout(binding = 1) uniform sampler2D shadowMap;
layout(binding = 2) uniform sampler2DArray materialTextures;

vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 toSrgb(vec3 c) { return pow(c, vec3(1.0 / 2.2)); }

// Aproximarea ACES (Narkowicz).
vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float shadowFactor(vec3 world, vec3 n)
{
    if (frame.shadow.x < 0.5)
        return 1.0;
    // Mutăm puțin punctul pe normală, ca fața să nu se umbrească singură („acnee”).
    vec4 lp = frame.lightViewProj * vec4(world + n * frame.shadow.y, 1.0);
    vec3 ndc = lp.xyz / lp.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    if (frame.params.y < 0.5)
        uv.y = 1.0 - uv.y;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0)
        return 1.0;
    float depth = frame.viewport.w > 0.5 ? ndc.z : ndc.z * 0.5 + 0.5;
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float stored = texture(shadowMap, uv + vec2(x, y) * frame.shadow.z).r;
            lit += depth - 0.0015 <= stored ? 1.0 : 0.0;
        }
    }
    return mix(1.0, lit / 9.0, frame.shadow.w);
}

void main()
{
    // Secțiunea: ce e de partea normalei planului nu se vede.
    if (dot(frame.section.xyz, frame.section.xyz) > 0.5 && dot(frame.section.xyz, v_world) - frame.section.w > 0.0)
        discard;
    vec3 n = normalize(v_normal);
    vec3 toEye = frame.forward.w > 0.5 ? -frame.forward.xyz : normalize(frame.eye.xyz - v_world);
    bool front = dot(n, toEye) >= 0.0;
    if (!front)
        n = -n;
    int style = int(frame.style.x + 0.5);

    // Linii ascunse: fețele au culoarea fundalului (doar acoperă ce e în spate).
    if (style == 2) {
        fragColor = vec4(frame.bgBottom.rgb, 1.0);
        return;
    }

    // Versoul cu material propriu îl folosește pe acela; altfel, ca până acum (materialul feței sau culoarea versoului).
    bool backMaterial = !front && v_backColor.a >= 0.0 && style != 1;
    bool hasMaterial = (backMaterial || v_color.a >= 0.0) && style != 1;
    vec4 surfaceColor = backMaterial ? v_backColor : v_color;
    vec3 base = hasMaterial ? surfaceColor.rgb : (front ? frame.faceFront.rgb : frame.faceBack.rgb);
    if (style == 1)
        base = vec3(0.92, 0.91, 0.89); // „clay”: alb mat, fără materiale
    float roughness = hasMaterial ? v_material.x : 0.85;
    float metallic = hasMaterial ? v_material.y : 0.0;

    vec3 albedo = toLinear(base);
    // Textura materialului (doar pe față, nu pe un verso cu material propriu), nuanțată de culoarea lui.
    if (v_uv.z > -0.5 && !backMaterial && style != 1)
        albedo *= toLinear(texture(materialTextures, v_uv).rgb);
    vec3 l = normalize(frame.lightDir.xyz);
    float hemi = 0.5 + 0.5 * n.z;
    vec3 sky = mix(vec3(0.42, 0.40, 0.38), vec3(0.62, 0.68, 0.78), hemi);
    float sun = max(dot(n, l), 0.0) * frame.lightDir.w * shadowFactor(v_world, n);
    vec3 diffuse = albedo * (sky * 0.55 + vec3(1.0, 0.97, 0.92) * sun * 0.95) * (1.0 - metallic * 0.6);

    // Strălucire (Blinn-Phong) mai îngustă pentru suprafețe netede; metalele o colorează.
    vec3 h = normalize(l + toEye);
    float shininess = mix(256.0, 8.0, roughness);
    float spec = pow(max(dot(n, h), 0.0), shininess) * (1.0 - roughness) * sun;
    vec3 specColor = mix(vec3(0.04), albedo, metallic);
    vec3 color = diffuse + specColor * spec * 2.0;

    // Luminile modelului: atenuare netedă până la rază, con pentru spot, o singură față pentru suprafață.
    int count = int(frame.lightCount.x + 0.5);
    for (int i = 0; i < 8; ++i) {
        if (i >= count)
            break;
        int kind = int(frame.lightPos[i].w + 0.5);
        vec3 dir = normalize(frame.lightDirection[i].xyz);
        vec3 toLight;
        float att = 1.0;
        if (kind == 2) {
            toLight = -dir;
        } else {
            vec3 d = frame.lightPos[i].xyz - v_world;
            float dist = length(d);
            toLight = d / max(dist, 1e-3);
            float range = max(frame.lightColor[i].w, 1.0);
            float x = clamp(1.0 - dist / range, 0.0, 1.0);
            att = x * x;
            if (kind == 1) {
                float c = dot(-toLight, dir);
                att *= smoothstep(frame.lightDirection[i].w, frame.lightExtra[i].x, c);
            } else if (kind == 3) {
                // Suprafața luminează doar în fața ei; mărimea înmoaie apropierea.
                att *= clamp(dot(-toLight, dir) + 0.15, 0.0, 1.0);
                att *= clamp(dist / max(frame.lightExtra[i].y * 0.5, 1.0), 0.35, 1.0);
            }
        }
        float ndl = max(dot(n, toLight), 0.0) * att;
        if (ndl <= 0.0)
            continue;
        vec3 lc = frame.lightColor[i].rgb;
        vec3 hl = normalize(toLight + toEye);
        float sl = pow(max(dot(n, hl), 0.0), shininess) * (1.0 - roughness);
        color += albedo * lc * ndl * (1.0 - metallic * 0.6) + specColor * lc * sl * ndl * 2.0;
    }

    vec3 lit = toSrgb(aces(color * 1.15));

    // Ceață după distanță.
    if (frame.style.y > 0.5) {
        float d = length(v_world - frame.eye.xyz);
        float f = smoothstep(frame.style.z, frame.style.w, d);
        lit = mix(lit, frame.bgBottom.rgb, f * 0.85);
    }

    // Starea 3: în afara grupului editat, estompat spre fundal.
    if (v_state > 2.5) {
        fragColor = vec4(mix(lit, frame.bgBottom.rgb, 0.62), 1.0);
        return;
    }
    // Tentă de accent: discretă la hover, mai puternică pentru selecție.
    float tint = v_state > 1.5 ? 0.38 : v_state > 0.5 ? 0.14 : 0.0;
    float alpha = hasMaterial ? clamp(surfaceColor.a, 0.0, 1.0) : 1.0;
    if (style == 4)
        alpha = 0.42; // x-ray
    fragColor = vec4(mix(lit, frame.accent.rgb, tint), alpha);
}
