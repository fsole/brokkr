<Shader Name="diffuse" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="albedo" Type="vec4"/>
      <Field Name="lightDirection" Type="vec4"/>
      <Field Name="fogParameters" Type = "vec4" />
    </Resource>
  <Resource Name = "MainTexture" Type = "texture2D" />
  </Resources>

  <Pass Name="OpaquePass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="None"/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;

      layout(location = 0) out vec3 normalWS;
      layout(location = 1) out vec2 uv;
      layout(location = 2) out vec3 positionWS;
      layout(location = 3) out vec3 cameraPositionWS;
      void main()
      {
        normalWS =  normalize( model.transform * vec4(aNormal,0.0) ).xyz;
        
        mat4 mvp = camera.viewProjection * model.transform;
        uv = aUV;
        positionWS = (model.transform * vec4(aPosition, 1.0)).xyz;
        cameraPositionWS = camera.viewToWorld[3].xyz;
        gl_Position =  mvp * vec4(aPosition,1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec3 normalWS;            
      layout(location = 1) in vec2 uv;
      layout(location = 2) in vec3 positionWS;
      layout(location = 3) in vec3 cameraPositionWS;

      layout(location = 0) out vec4 color;
      
      vec3 applyHalfSpaceFog(vec3 P, vec3 C, vec3 F, vec4 fogParameters, vec3 color)
      {
        vec3 V = C - P;

        float FdotV = dot(F, V);
        float FdotC = dot(F, C);
        float FdotP = dot(F, P);

        float k = 0.0;
        if (FdotC &lt;= 0.0)
          k = 1.0;


        float c1 = k * (FdotP + FdotC);
        float c2 = (1 - 2 * k) * (FdotP);
        float g = min(c2, 0.0);

        g = -fogParameters.a * length(V) * (c1 - g * g / abs(FdotV));
        float f = clamp(exp2(-g), 0, 1);

        return color.rgb * f + fogParameters.rgb * (1.0 - f);
      }

      void main()
      {
        vec4 diffuse = texture(MainTexture, uv);
        if (diffuse.a &lt; 0.2) discard;
        vec4 c = (max( 0, dot(normalWS, globals.lightDirection.xyz)) + 0.2) * globals.albedo * diffuse;
        color = vec4(applyHalfSpaceFog(positionWS, cameraPositionWS, vec3(0.0, 1.0f, 0.0), globals.fogParameters, c.rgb), c.a);
      }			
    </FragmentShader>
  </Pass>
</Shader>