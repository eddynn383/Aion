#version 440
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec3 v_side;
layout(location = 2) in vec3 v_along;
layout(location = 0) out vec4 fragColor;

void main()
{
    // Împărțirea la w dă valori liniare pe ecran (interpolarea e corectă perspectival).
    float side = abs(v_side.x / v_side.y);
    float coverage = clamp(v_side.z + 0.5 - side, 0.0, 1.0);

    float dash = v_along.z;
    if (dash > 0.0) {
        float along = v_along.x / v_along.y;
        float phase = mod(along, dash * 2.0);
        coverage *= clamp(dash - phase + 0.5, 0.0, 1.0);
    }
    if (coverage <= 0.0)
        discard;
    fragColor = vec4(v_color.rgb, v_color.a * coverage);
}
