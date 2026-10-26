#version 440
// Adâncimea (în spațiul ferestrei, 0..1) scrisă în canalul roșu al hărții de umbre.
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
}
