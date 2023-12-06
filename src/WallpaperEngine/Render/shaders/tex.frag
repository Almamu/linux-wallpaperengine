#version 150

in vec2 Texcoord;

out vec4 outputColor;

uniform sampler2D tex;

void main() {
  outputColor = texture2D(tex, Texcoord);
  if (outputColor.a < 0.1)
  {
    discard;
  }
}
