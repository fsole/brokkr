<Shader Name="diffuse" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="albedo" Type="vec4"/>
      <Field Name="lightDirection" Type="vec4"/>
    </Resource>
  </Resources>

  <Pass Name="OpaquePass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="Back"/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;

      layout(location = 0) out vec3 normalWS;

      void main()
      {
        normalWS =  normalize( model.transform * vec4(aNormal,0.0) ).xyz;
        mat4 mvp = camera.projection * camera.worldToView * model.transform;
        gl_Position =  mvp * vec4(aPosition,1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec3 normalWS;            
      layout(location = 0) out vec4 color;
                        
      void main()
      {
        color = (max( 0, dot(normalWS, globals.lightDirection.xyz)) + 0.1) * globals.albedo;
      }			
    </FragmentShader>
  </Pass>
</Shader>