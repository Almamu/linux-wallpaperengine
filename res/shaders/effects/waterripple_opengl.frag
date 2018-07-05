
// [COMBO_OFF] {"material":"ui_editor_properties_specular","combo":"SPECULAR","type":"options","default":0}

varying vec4 v_TexCoord;
varying vec2 v_Scroll;

uniform sampler2D g_Texture0; // {"material":"ui_editor_properties_framebuffer","hidden":true}
uniform sampler2D g_Texture1; // {"material":"ui_editor_properties_water_normal"}
uniform sampler2D g_Texture2; // {"material":"ui_editor_properties_opacity_mask","mode":"opacitymask","default":"util/white"}

uniform float g_Strength; // {"material":"ui_editor_properties_ripple_strength","default":0.1,"range":[0,1]}
uniform float g_SpecularPower; // {"material":"ui_editor_properties_ripple_specular_power","default":1.0,"range":[0,100]}
uniform float g_SpecularStrength; // {"material":"ui_editor_properties_ripple_specular_strength","default":1.0,"range":[0,10]}
uniform vec3 g_SpecularColor; // {"material":"ui_editor_properties_ripple_specular_color","default":"1 1 1","type":"color"}

varying vec4 v_TexCoordRipple;

void main() {
	vec2 texCoord = v_TexCoord.xy;
	
	float mask = texSample2D(g_Texture2, v_TexCoord.zw).r;
	
	vec3 n1 = texSample2D(g_Texture1, v_TexCoordRipple.xy).xyz * 2 - 1;
	vec3 n2 = texSample2D(g_Texture1, v_TexCoordRipple.zw).xyz * 2 - 1;
	vec3 normal = normalize(vec3(n1.xy + n2.xy, n1.z));
	
	texCoord.xy += normal.xy * g_Strength * g_Strength * mask;
	
	gl_FragColor = texSample2D(g_Texture0, texCoord);
	
#if SPECULAR == 1
	vec2 direction = vec2(0.5, 0.0) - v_TexCoord.xy;
	direction = normalize(direction);
	float specular = max(0.0, dot(normal.xy, direction)) * max(0.0, dot(direction, vec2(0.0, -1.0)));
	
	specular = pow(specular, g_SpecularPower) * g_SpecularStrength;
	gl_FragColor.rgb += specular * g_SpecularColor * gl_FragColor.a;
#endif
}
