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
