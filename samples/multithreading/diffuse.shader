<Shader Name="diffuse" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="albedo" Type="vec4"/>
      <Field Name="lightDirection" Type="vec4"/>
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
      void main()
      {
        normalWS =  normalize( model.transform * vec4(aNormal,0.0) ).xyz;
        mat4 mvp = camera.viewProjection * model.transform;
        uv = aUV;
        gl_Position =  mvp * vec4(aPosition,1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec3 normalWS;            
      layout(location = 1) in vec2 uv;
      layout(location = 0) out vec4 color;
      
                        
      void main()
      {
        vec4 diffuse = texture(MainTexture, uv);
        if (diffuse.a &lt; 0.2) discard;
        color = (max( 0, dot(normalWS, globals.lightDirection.xyz)) + 0.2) * globals.albedo * diffuse;
      }			
    </FragmentShader>
  </Pass>
</Shader>