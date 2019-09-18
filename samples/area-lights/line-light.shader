<Shader Name="line-light" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="colorBegin" Type="vec4"/> 
      <Field Name="colorEnd" Type="vec4"/> 
      <Field Name="radius" Type="float"/>
    </Resource>
    <Resource Name="albedoRoughnessRT" Type="texture2D"/>
    <Resource Name="emissionRT" Type="texture2D"/>
    <Resource Name="normalDepthRT" Type="texture2D"/>
  </Resources>

  <Pass Name="OpaquePass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="Back"/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;

      layout(location = 0) out vec3 normalVS;
      layout(location = 1) out vec4 lightColor;
      
      void main()
      {
        mat4 mv = camera.worldToView * model.transform;		
        normalVS =  normalize( mv * vec4(aNormal,0.0) ).xyz;
        lightColor = mix(globals.colorBegin, globals.colorEnd, aPosition.z + 0.5);
        gl_Position = camera.projection * mv * vec4(aPosition,1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec3 normalVS;
      layout(location = 1) in vec4 lightColor;
      
      layout(location = 0) out vec4 color;
      layout(location = 1) out vec4 emission;
      layout(location = 2) out vec4 normalDepth;
                        
      void main()
      {
        color = vec4(0,0,0,0);
        normalDepth = vec4(normalVS, gl_FragCoord.z);
        emission = lightColor;
      }			
    </FragmentShader>
  </Pass>
  
  <Pass Name="LightPass">
    <ZWrite Value="Off"/>
    <ZTest Value="Always"/>
    <Cull Value="Off"/>
    <Blend Target="0" ColorBlendOperation="Add" ColorSrcFactor="One" ColorDstFactor="One" />
    

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;
      
      layout(location = 0) out vec3 linePointsVS[2];
      
      void main()
      {
        mat4 mv = camera.worldToView * model.transform;
        linePointsVS[0] = (mv * vec4(0.0,0.0,-0.5,1.0) ).xyz;
        linePointsVS[1] = (mv * vec4(0.0,0.0, 0.5,1.0) ).xyz;
        
        gl_Position = camera.projection * camera.worldToView * vec4(aPosition * globals.radius + model.transform[3].xyz, 1.0);
      }
    </VertexShader>
    
    <FragmentShader>            
    
      layout(location = 0) in vec3 linePointsVS[2];

      layout(location = 0) out vec4 color;
      
      vec3 viewSpacePositionFromDepth(vec2 uv, float depth)
      {
        vec3 clipSpacePosition = vec3(uv* 2.0 - 1.0, depth);
        vec4 viewSpacePosition = camera.projectionInverse * vec4(clipSpacePosition,1.0);
        return(viewSpacePosition.xyz / viewSpacePosition.w);        
      }
      
      // Get Most Representative Point  in the light for the diffuse part
      vec3 GetMRPDiffuse(vec3 P, vec3 A, vec3 B, out float t)
      {
        vec3 PA = A - P; 
        vec3 PB = B - P;
        vec3 AB = B - A;
        float a = length(PA);
        float b = length(PB);
        t = clamp((a / (b + a)), 0.0, 1.0);
        return A + AB * t;
      }

      // Get Most Representative Point in the light for the specular part
      vec3 GetMRPSpecular(vec3 P, vec3 A, vec3 B, vec3 R, out float t)
      {
        vec3 PA = A - P;
        vec3 PB = B - P;
        vec3 AB = B - A;

        // If R is normalized, dot(R, R) = 1 and can be optimized away.
        // After that dot(AB, P) - dot(AB, A) == dot(AB, P - A)
        float t_num = dot(R, A) * dot(AB, R) + dot(AB, P) * dot(R, R) - dot(R, P) * dot(AB, R) - dot(AB, A) * dot(R, R);
        float t_denom = dot(AB, AB) * dot(R, R) - dot(AB, R) * dot(AB, R);
        t = clamp(t_num / t_denom, 0.0, 1.0);

        return A + AB * t;
      }

      const float pi = 3.14159265;
      void main()
      {
        vec2 uv = gl_FragCoord.xy / textureSize(albedoRoughnessRT, 0);        
        vec4 normalDepth = texture( normalDepthRT, uv );
        if( normalDepth.w &gt; gl_FragCoord.z ) 
          discard;
        
        vec3 fragPositionVS = viewSpacePositionFromDepth(uv, normalDepth.w );                
        vec4 albedoRoughness = texture(albedoRoughnessRT, uv);

        //Diffuse term
        float t = 0.0;
        vec3 diffuseMostRepresentativePoint = GetMRPDiffuse(fragPositionVS, linePointsVS[0], linePointsVS[1], t);
        vec3 L = diffuseMostRepresentativePoint - fragPositionVS;
        float d = length(L);
        L /= d;
        vec3 N = normalize(normalDepth.xyz);        
        vec3 diffuse = albedoRoughness.rgb * max(0.0, dot(N, L));
        float falloff = 1.0 / d * d;
        vec3 lightDiffuseIntensity = mix(globals.colorBegin.rgb, globals.colorEnd.rgb, t) * falloff;

        //specular term
        vec3 V = -normalize(fragPositionVS);
        vec3 R = reflect(V, N);
        vec3 specularMostRepresentativePoint = GetMRPSpecular(fragPositionVS, linePointsVS[0], linePointsVS[1], R, t);
        L = specularMostRepresentativePoint - fragPositionVS;
        d = length(L);
        L /= d;                
        vec3 H = normalize(V + L);        
        float NdotH = max(0, dot(N, H));
        float NdotL = max(0, dot(N, L));
        vec3 specular = vec3(pow(NdotH, pow(1000.0, 1.0 - albedoRoughness.w))) * NdotL;
        falloff = 1.0 / d * d;
        vec3 lightSpecularIntensity = mix(globals.colorBegin.rgb, globals.colorEnd.rgb, t) * falloff;
        
        vec3 c = diffuse * lightDiffuseIntensity + specular * lightSpecularIntensity;
        c /= 2.0*pi;
        c += texture(emissionRT, uv).rgb;

        color = vec4(c, 1.0);
      }			
    </FragmentShader>
  </Pass>  
</Shader>