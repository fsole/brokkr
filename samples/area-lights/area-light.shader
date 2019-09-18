<Shader Name="area-light" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="color" Type="vec4"/>
      <Field Name="radius" Type="float"/>
    </Resource>
    <Resource Name="albedoRoughnessRT" Type="texture2D"/>
    <Resource Name="emissionRT" Type="texture2D"/>
    <Resource Name="normalDepthRT" Type="texture2D"/>
    <Resource Name="ltcAmpTexture" Type="texture2D"/>
    <Resource Name="ltcMatTexture" Type="texture2D"/>
  </Resources>

  <Pass Name="OpaquePass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="Off"/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;
      layout(location = 0) out vec3 normalVS;
      
      void main()
      {
        mat4 mv = camera.worldToView * model.transform;		
        normalVS =  normalize( mv * vec4(aNormal,0.0) ).xyz;
        gl_Position = camera.projection * mv * vec4(aPosition,1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec3 normalVS;
      
      layout(location = 0) out vec4 color;
      layout(location = 1) out vec4 emission;
      layout(location = 2) out vec4 normalDepth;
                        
      void main()
      {
        color = vec4(0,0,0,0);
        normalDepth = vec4(normalVS, gl_FragCoord.z);
        emission = globals.color;
      }			
    </FragmentShader>
  </Pass>
  
  <Pass Name="LightPass">
    <ZWrite Value="Off"/>
    <ZTest Value="Always"/>
    <Cull Value="Front"/>
    <Blend Target="0" ColorBlendOperation="Add" ColorSrcFactor="One" ColorDstFactor="One" />

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;

      layout(location = 0) out vec3 rectPointsVS[4];
      
      void main()
      {
        mat4 mv = camera.worldToView * model.transform;

        rectPointsVS[0] = (mv * vec4(-0.5, -0.5, 0.0, 1.0)).xyz;
        rectPointsVS[1] = (mv * vec4( 0.5, -0.5, 0.0, 1.0)).xyz;
        rectPointsVS[2] = (mv * vec4( 0.5,  0.5, 0.0, 1.0)).xyz;
        rectPointsVS[3] = (mv * vec4(-0.5,  0.5, 0.0, 1.0)).xyz;
        
        gl_Position = camera.projection * camera.worldToView * vec4(aPosition * globals.radius + model.transform[3].xyz, 1.0);
      }
    </VertexShader>
    
    <FragmentShader>            
    
      layout(location = 0) in vec3 rectPointsVS[4];
      layout(location = 0) out vec4 color;
      
      vec3 viewSpacePositionFromDepth(vec2 uv, float depth)
      {
        vec3 clipSpacePosition = vec3(uv* 2.0 - 1.0, depth);
        vec4 viewSpacePosition = camera.projectionInverse * vec4(clipSpacePosition,1.0);
        return(viewSpacePosition.xyz / (viewSpacePosition.w+0.00001));
      }
      
      void ClipQuadToHorizon(inout vec3 L[5], out int n)
      {
        // detect clipping config
        int config = 0;
        if (L[0].z > 0.0) config += 1;
        if (L[1].z > 0.0) config += 2;
        if (L[2].z > 0.0) config += 4;
        if (L[3].z > 0.0) config += 8;

        // clip
        n = 0;

        if (config == 0)
        {
          // clip all
        }
        else if (config == 1) // V1 clip V2 V3 V4
        {
          n = 3;
          L[1] = -L[1].z * L[0] + L[0].z * L[1];
          L[2] = -L[3].z * L[0] + L[0].z * L[3];
        }
        else if (config == 2) // V2 clip V1 V3 V4
        {
          n = 3;
          L[0] = -L[0].z * L[1] + L[1].z * L[0];
          L[2] = -L[2].z * L[1] + L[1].z * L[2];
        }
        else if (config == 3) // V1 V2 clip V3 V4
        {
          n = 4;
          L[2] = -L[2].z * L[1] + L[1].z * L[2];
          L[3] = -L[3].z * L[0] + L[0].z * L[3];
        }
        else if (config == 4) // V3 clip V1 V2 V4
        {
          n = 3;
          L[0] = -L[3].z * L[2] + L[2].z * L[3];
          L[1] = -L[1].z * L[2] + L[2].z * L[1];
        }
        else if (config == 5) // V1 V3 clip V2 V4) impossible
        {
          n = 0;
        }
        else if (config == 6) // V2 V3 clip V1 V4
        {
          n = 4;
          L[0] = -L[0].z * L[1] + L[1].z * L[0];
          L[3] = -L[3].z * L[2] + L[2].z * L[3];
        }
        else if (config == 7) // V1 V2 V3 clip V4
        {
          n = 5;
          L[4] = -L[3].z * L[0] + L[0].z * L[3];
          L[3] = -L[3].z * L[2] + L[2].z * L[3];
        }
        else if (config == 8) // V4 clip V1 V2 V3
        {
          n = 3;
          L[0] = -L[0].z * L[3] + L[3].z * L[0];
          L[1] = -L[2].z * L[3] + L[3].z * L[2];
          L[2] = L[3];
        }
        else if (config == 9) // V1 V4 clip V2 V3
        {
          n = 4;
          L[1] = -L[1].z * L[0] + L[0].z * L[1];
          L[2] = -L[2].z * L[3] + L[3].z * L[2];
        }
        else if (config == 10) // V2 V4 clip V1 V3) impossible
        {
          n = 0;
        }
        else if (config == 11) // V1 V2 V4 clip V3
        {
          n = 5;
          L[4] = L[3];
          L[3] = -L[2].z * L[3] + L[3].z * L[2];
          L[2] = -L[2].z * L[1] + L[1].z * L[2];
        }
        else if (config == 12) // V3 V4 clip V1 V2
        {
          n = 4;
          L[1] = -L[1].z * L[2] + L[2].z * L[1];
          L[0] = -L[0].z * L[3] + L[3].z * L[0];
        }
        else if (config == 13) // V1 V3 V4 clip V2
        {
          n = 5;
          L[4] = L[3];
          L[3] = L[2];
          L[2] = -L[1].z * L[2] + L[2].z * L[1];
          L[1] = -L[1].z * L[0] + L[0].z * L[1];
        }
        else if (config == 14) // V2 V3 V4 clip V1
        {
          n = 5;
          L[4] = -L[0].z * L[3] + L[3].z * L[0];
          L[0] = -L[0].z * L[1] + L[1].z * L[0];
        }
        else if (config == 15) // V1 V2 V3 V4
        {
          n = 4;
        }

        if (n == 3)
          L[3] = L[0];
        if (n == 4)
          L[4] = L[0];
      }

      float IntegrateEdge(vec3 v1, vec3 v2)
      {
        float cosTheta = dot(v1, v2);
        float theta = acos(cosTheta);
        float res = cross(v1, v2).z * ((theta &gt; 0.001) ? theta / sin(theta) : 1.0);

        return res;
      }

      vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
      {
        // construct orthonormal basis around N
        vec3 T1, T2;
        T1 = normalize(V - N * dot(V, N));
        T2 = cross(N, T1);

        // rotate area light in (T1, T2, N) basis
        Minv = Minv * transpose(mat3(T1, T2, N));

        // polygon (allocate 5 vertices for clipping)
        vec3 L[5];
        L[0] = Minv * (points[0] - P);
        L[1] = Minv * (points[1] - P);
        L[2] = Minv * (points[2] - P);
        L[3] = Minv * (points[3] - P);

        int n;
        ClipQuadToHorizon(L, n);

        if (n == 0)
          return vec3(0, 0, 0);

        // project onto sphere
        L[0] = normalize(L[0]);
        L[1] = normalize(L[1]);
        L[2] = normalize(L[2]);
        L[3] = normalize(L[3]);
        L[4] = normalize(L[4]);

        // integrate
        float sum = 0.0;

        sum += IntegrateEdge(L[0], L[1]);
        sum += IntegrateEdge(L[1], L[2]);
        sum += IntegrateEdge(L[2], L[3]);
        if (n >= 4)
          sum += IntegrateEdge(L[3], L[4]);
        if (n == 5)
          sum += IntegrateEdge(L[4], L[0]);

        sum = twoSided ? abs(sum) : max(0.0, sum);

        vec3 Lo_i = vec3(sum, sum, sum);

        return Lo_i;
      }

      const float LUT_SIZE = 64.0;
      const float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
      const float LUT_BIAS = 0.5 / LUT_SIZE;
      const float pi = 3.14159265;

      void main()
      {
        vec2 uv = gl_FragCoord.xy / textureSize(albedoRoughnessRT, 0);        
        vec4 normalDepth = texture( normalDepthRT, uv );
        if( normalDepth.w &gt; gl_FragCoord.z )
          discard;
        
        vec3 pos = viewSpacePositionFromDepth(uv, normalDepth.w );                
        vec4 albedoRoughness = texture(albedoRoughnessRT, uv);

        vec3 N = normalize(normalDepth.xyz);
        vec3 V = -normalize(pos);

        float theta = acos(dot(N, V));
        vec2 uv2 = vec2(albedoRoughness.w + 0.05, theta / (0.5*pi));
        uv2 = uv2 * LUT_SCALE + LUT_BIAS;

        vec4 t = texture(ltcMatTexture, uv2);
        mat3 Minv = mat3(
          vec3(1, 0, t.y),
          vec3(0, t.z, 0),
          vec3(t.w, 0, t.x)
        );

        vec3 spec = LTC_Evaluate(N, V, pos, Minv, rectPointsVS, true);
        spec *= texture(ltcAmpTexture, uv2).g;
        vec3 diff = LTC_Evaluate(N, V, pos, mat3(1), rectPointsVS, true);
        vec3 c = globals.color.rgb * (spec + albedoRoughness.rgb*diff);
        c /= 2.0*pi;
        c += texture(emissionRT, uv).rgb;
        color = vec4(c, 1.0);

      }			
    </FragmentShader>
  </Pass>  
</Shader>