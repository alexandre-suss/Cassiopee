#version 400 compatibility

/*
    Phong (two sides) + shadow
*/
in V2F_OUT
{
    vec4 position;
    vec4 mv_position;
    vec4 mvp_position;
    vec4 view_normal;
    vec4 nrm_view_normal;
    vec4 color;
    vec4 vdata1, vdata2, vdata3, vdata4;
} v2f_out;

uniform float specularFactor;
uniform float diffuseFactor;
uniform int shadow;
uniform sampler2D ShadowMap;

void main (void)
{ 
  vec3 N = normalize(v2f_out.view_normal.xyz);
  vec3 E = normalize(-v2f_out.mv_position.xyz);
  vec3 L = normalize(gl_LightSource[0].position.xyz-v2f_out.mv_position.xyz);
  float dotNL = dot(N, L);
  if (dotNL < 0.) { N = -N; dotNL = -dotNL; }

  vec3 R = normalize(-reflect(L,N));
  vec4 Iamb = gl_LightSource[0].ambient; 
  vec4 Idiff = gl_LightSource[0].diffuse * diffuseFactor * max(dotNL, 0.0);
  vec4 Ispec = (specularFactor*specularFactor)*gl_LightSource[0].specular*pow(max(dot(R,E),0.0),0.25*gl_FrontMaterial.shininess);
  vec4 col = Iamb + v2f_out.color*Idiff + Ispec;
  col = clamp(col, 0., 1.);
  float shadowValue = 1.;

  if (shadow > 0)
  {
  // Coords -> texCoords
  vec4 ShadowCoord = gl_TextureMatrix[0] * v2f_out.position;
  vec4 shadowCoordinateW = ShadowCoord / ShadowCoord.w;

  // Used to lower moire pattern and self-shadowing
  //shadowCoordinateW.z -= 0.00001;
  shadowCoordinateW.z -= (abs(dotNL)+0.1)*0.00001;

  // Z buffer du point dans la texture rendu du pt de vue de la lumiere
  float distanceFromLight = texture2D(ShadowMap, shadowCoordinateW.st).r;
  float s = shadowCoordinateW.s;
  float t = shadowCoordinateW.t;      
  if (ShadowCoord.w > 0.0 && s > 0.001 && s < 0.999 && t > 0.001 && t < 0.999)
    {
      //shadowValue = distanceFromLight < shadowCoordinateW.z ? 0.5 : 1.0;
      //if (distanceFromLight < shadowCoordinateW.z - 0.001) shadowValue = 0.5;
      //else if (distanceFromLight >= shadowCoordinateW.z) shadowValue = 1.;
      //else shadowValue = 0.5/0.001*distanceFromLight+(1.-0.5/0.001)*shadowCoordinateW.z;
      if (distanceFromLight < shadowCoordinateW.z - 0.001) shadowValue = 0.5;
      else if (distanceFromLight >= shadowCoordinateW.z) shadowValue = 1.;
      else shadowValue = 500.*distanceFromLight-499.*shadowCoordinateW.z;
      
    }  
  }
  
  gl_FragColor = shadowValue * col;
  gl_FragColor.a = v2f_out.color.a;
}
