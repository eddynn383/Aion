#version 440
// Un singur triunghi care acoperă tot ecranul (fără vertex buffer).
layout(location = 0) out vec2 v_ndc;

void main()
{
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
    v_ndc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
