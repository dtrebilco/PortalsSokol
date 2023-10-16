@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs vs
out vec2 texCoord;
out vec3 lightVec;
out vec3 viewVec;

uniform vs_params {
    mat4 mvp;
    vec3 lightPos;
    vec3 camPos;
};

in vec4 position;
in vec2 uv;
in vec3 mat0;
in vec3 mat1;
in vec3 mat2x;

void main() {

  gl_Position = mvp * position;

  texCoord = uv.xy;

  vec3 lVec = lightPos - position.xyz;
  lightVec.x = dot(mat0.xyz, lVec);
  lightVec.y = dot(mat1.xyz, lVec);
  lightVec.z = dot(mat2x.xyz, lVec);

  vec3 vVec = camPos - position.xyz;
  viewVec.x = dot(mat0.xyz, vVec);
  viewVec.y = dot(mat1.xyz, vVec);
  viewVec.z = dot(mat2x.xyz, vVec);
}
@end

@fs fs
uniform texture2D Base;
uniform texture2D Bump;
uniform sampler smp;

uniform fs_params {
  float invRadius;
  float ambient;
};

in vec2 texCoord;
in vec3 lightVec;
in vec3 viewVec;

out vec4 frag_color;

void main(){

	vec4 base = texture(sampler2D(Base,smp), texCoord);
	vec3 bump = texture(sampler2D(Bump,smp), texCoord).xyz * 2.0 - 1.0;

	bump = normalize(bump);

	float distSqr = dot(lightVec, lightVec);
	vec3 lVec = lightVec * inversesqrt(distSqr);

	float atten = clamp(1.0 - invRadius * sqrt(distSqr), 0.0, 1.0);
	float diffuse = clamp(dot(lVec, bump), 0.0, 1.0);

	float specular = pow(clamp(dot(reflect(normalize(-viewVec), bump), lVec), 0.0, 1.0), 16.0);
	
	frag_color = ambient * base + (diffuse * base + 0.6 * specular) * atten;
}
@end

@program shd vs fs

@vs vs_pfx
uniform vs_params_pfx {
    mat4 mvp;
};

in vec4 position;
in vec2 in_uv;
in vec4 in_color;
out vec2 uv;
out vec4 color;

void main() {
  // Position it
  gl_Position = mvp * position;

  uv = in_uv;
  color = in_color;
}
@end

@fs fs_pfx
out vec4 frag_color;
in vec2 uv;
in vec4 color;
uniform texture2D tex0;
uniform sampler smp;

void main() {
  frag_color = texture(sampler2D(tex0,smp), uv);
  frag_color *= color;
}
@end

@program shd_pfx vs_pfx fs_pfx
