<Shader Name="pbr" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="albedo" Type="vec3"/>
      <Field Name="metallic" Type="float"/>
      <Field Name="F0" Type="vec3"/>
      <Field Name="roughness" Type="float"/>
    </Resource>
    <Resource Name="lights" Type="storage_buffer" Shared="yes"> 			
      <Field Name="count" Type="int"/>
      <Field Name="intensity" Type="float"/>
      <Field Name="data" Type="compound_type" Count="">
        <Field Name="position" Type="vec4"/>
        <Field Name="color" Type="vec3"/>
        <Field Name="radius" Type="float"/>
      </Field>
    </Resource>
        
    <Resource Name="irradianceMap" Type="textureCube"/>
    <Resource Name="specularMap" Type="textureCube"/>
    <Resource Name="brdfLUT" Type="texture2D"/>        
  </Resources>


  <Pass Name="OpaquePass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="Back"/>
    <Blend Target="0" SrcColor="SrcAlpha" DstColor="OneMinusSrcAlpha" SrcAlpha="" DstAlpha=""/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;
            
      layout(location = 0) out vec4 positionVS;
      layout(location = 1) out vec3 normalVS;
      void main()
      {
        mat4 mv = camera.worldToView * model.transform;
        positionVS =  mv * vec4(aPosition,1.0);
        mat4 normalMatrix = transpose( inverse(mv) );
        normalVS =  normalize( (normalMatrix * vec4(aNormal,0.0) ).xyz );
        mat4 mvp = camera.viewProjection * model.transform;
        gl_Position =  mvp * vec4(aPosition,1.0);
      }
    </VertexShader>
        
    <FragmentShader>         
      const float PI = 3.14159265359;
      
      vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
      {
        return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
      }
            
      float DistributionGGX(vec3 N, vec3 H, float roughness)
      {
        float a = roughness*roughness;
        float a2 = a*a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH*NdotH;
        float nom = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return nom / denom;
      }

      float GeometrySchlickGGX(float NdotV, float roughness)
      {
        float r = (roughness + 1.0);
        float k = (r*r) / 8.0;
        float nom = NdotV;
        float denom = NdotV * (1.0 - k) + k;
        return nom / denom;
      }

      float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
      {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float ggx2 = GeometrySchlickGGX(NdotV, roughness);
        float ggx1 = GeometrySchlickGGX(NdotL, roughness);
        return ggx1 * ggx2;
      }
            
      vec3 applyLight(vec4 position, vec3 N, vec3 V, vec3 albedo, vec3 kS, vec3 kD, float roughness, vec4 lightPosition, vec3 lightColor, float lightRadius)
      {
        vec3 L = (lightPosition - position ).xyz;
        float lightDistance = length(L);
        L /= lightDistance;                
        vec3 H = normalize(V + L);
                
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float NdotL = max( dot(N,L), 0.0 );
        float NdotV = max( dot(N,V), 0.0 );
                
        vec3 nominator = NDF * G * kS;
        float denominator = (4.0 * NdotV * NdotL) + 0.00001;
        vec3 specular = nominator / denominator;

        float attenuation = pow(1.0 - clamp( lightDistance / lightRadius, 0.0, 1.0), 2);
        vec3 lightRadiance = lightColor * attenuation * NdotL;
        return (kD * albedo / PI + specular) * lightRadiance;
      }
            
      layout(location = 0) in vec4 positionVS;
      layout(location = 1) in vec3 normalVS;
            
      layout(location = 0) out vec4 color;
      void main()
      {
        vec3 N = normalize(normalVS);
        vec3 V = normalize(-positionVS.xyz);
        float NdotV = max(dot(N, V), 0.0);
        vec3 F = fresnelSchlickRoughness( NdotV, globals.F0, globals.roughness);
        vec3 kD = max( vec3(0), vec3(1.0) - F );
        kD *= 1.0 - globals.metallic;

        vec3 c = vec3(0,0,0);                
        for( int i = 0; i &lt; lights.count; ++i )
        {
          vec4 lightVS = camera.worldToView * lights.data[i].position;
          c += applyLight( positionVS, N, V, globals.albedo, F, kD, globals.roughness, lightVS, lights.data[i].color, lights.data[i].radius ) * lights.intensity;
        }

        //Image based lighting
        const float MAX_REFLECTION_LOD = 4;
        vec3 normalWS = normalize( vec4( camera.viewToWorld * vec4(N,0.0) ).xyz);
        vec3 irradiance = texture(irradianceMap, normalWS).rgb;
        vec3 diffuse = irradiance * globals.albedo;        
        vec3 reflection = reflect(-V, N);
        vec3 reflectionWS = normalize( vec4( camera.viewToWorld * vec4(reflection,0.0) ).xyz);
        vec3 prefilteredColor = textureLod(specularMap, reflectionWS,  min(globals.roughness * MAX_REFLECTION_LOD,MAX_REFLECTION_LOD)).rgb;  
        vec2 envBRDF  = texture(brdfLUT, vec2(NdotV, globals.roughness)).rg;
        vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
        vec3 ibl = kD * diffuse + specular;

        color = vec4( c + ibl, 1.0);
      }
    </FragmentShader>
  </Pass>	
</Shader>