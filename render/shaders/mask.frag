#version 440
// Masca selecției: 1 unde se vede o față selectată, 0 în rest (pentru conturul din outline.frag).
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world;
layout(location = 2) in float v_state;
layout(location = 0) out vec4 fragColor;

void main()
{
    // Stările: 0 normal, 1 sub cursor, 2 selectat, 3 estompat (în afara contextului de editare).
    fragColor = vec4(v_state > 1.5 && v_state < 2.5 ? 1.0 : 0.0, 0.0, 0.0, 1.0);
}
