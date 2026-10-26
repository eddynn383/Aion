#version 440
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec3 v_local;
layout(location = 0) out vec4 fragColor;

void main()
{
    float coverage = clamp(v_local.z + 0.5 - length(v_local.xy), 0.0, 1.0);
    if (coverage <= 0.0)
        discard;
    fragColor = vec4(v_color.rgb, v_color.a * coverage);
}
