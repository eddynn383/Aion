#version 440
// Depth of field (plan Animare, secțiunea 9): o estompare separabilă (o trecere orizontală, una verticală) cu
// greutăți după cercul de confuzie al fiecărui pixel, din adâncimea liniară (gbuffer.frag). Un punct contribuie dacă
// cercul lui ajunge până aici; unul din spate, doar cât cercul pixelului — așa obiectele clare nu „curg” peste fundal,
// iar fundalul neclar nu trece peste ele.
layout(location = 0) in vec2 v_ndc;
layout(location = 0) out vec4 fragColor;

// Trebuie să corespundă cu DofUniforms din render/src/ViewportRenderer.cpp.
layout(std140, binding = 0) uniform Dof {
    vec4 lens;   // x = distanța de focus (mm), y = focala (mm), z = diafragma (f/N), w = pixeli pe mm de senzor
    vec4 pass;   // xy = direcția estompării (1, 0) sau (0, 1), z = raza maximă (px)
    vec4 size;   // xy = mărimea țintei în pixeli
} dof;

layout(binding = 1) uniform sampler2D colorTexture;
layout(binding = 2) uniform sampler2D gbuffer;

const int kMaxRadius = 16;

// Raza cercului de confuzie (pixeli) la adâncimea dată; fundalul (fără geometrie) e la infinit.
float coc(float depth)
{
    float d = depth > 0.0 ? depth : 1.0e9;
    float f = dof.lens.y;
    float s = max(dof.lens.x, f * 1.001);
    float c = f / max(dof.lens.z, 0.1) * f / (s - f) * abs(s - d) / d;
    return min(c * dof.lens.w * 0.5, dof.pass.z);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / dof.size.xy;
    float depth0 = texture(gbuffer, uv).a;
    float d0 = depth0 > 0.0 ? depth0 : 1.0e9;
    float c0 = coc(depth0);
    vec4 sum = vec4(texture(colorTexture, uv).rgb, 1.0);
    int radius = int(ceil(min(dof.pass.z, float(kMaxRadius))));
    for (int i = -kMaxRadius; i <= kMaxRadius; ++i) {
        if (i == 0 || abs(i) > radius)
            continue;
        vec2 suv = uv + dof.pass.xy * float(i) / dof.size.xy;
        float depth = texture(gbuffer, suv).a;
        float d = depth > 0.0 ? depth : 1.0e9;
        float ci = coc(depth);
        // Ce e în spate nu se întinde peste un pixel mai clar decât el.
        float reach = d > d0 ? min(ci, c0) : ci;
        float w = clamp(reach - float(abs(i)) + 1.0, 0.0, 1.0);
        sum += vec4(texture(colorTexture, suv).rgb, 1.0) * w;
    }
    fragColor = vec4(sum.rgb / sum.a, 1.0);
}
