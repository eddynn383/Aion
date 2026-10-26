#version 440
// Iluminare simplă: emisferică (cer/sol) + o lumină direcțională moale.
// Fața din spate are altă culoare, ca în SketchUp.
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world;
layout(location = 2) in float v_state;
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
    vec3 n = normalize(v_normal);
    vec3 toEye = normalize(frame.eye.xyz - v_world);
    bool front = dot(n, toEye) >= 0.0;
    if (!front)
        n = -n;
    vec3 base = front ? frame.faceFront.rgb : frame.faceBack.rgb;

    float hemi = 0.5 + 0.5 * n.z;
    vec3 ambient = mix(vec3(0.55, 0.53, 0.50), vec3(0.80, 0.84, 0.90), hemi) * 0.55;
    float diffuse = max(dot(n, frame.lightDir.xyz), 0.0) * 0.6;
    vec3 lit = base * (ambient + diffuse);
    // Tentă de accent: discretă la hover, mai puternică pentru selecție.
    float tint = v_state > 1.5 ? 0.38 : v_state > 0.5 ? 0.14 : 0.0;
    fragColor = vec4(mix(lit, frame.accent.rgb, tint), 1.0);
}
