<Shader Name="simple" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">
      <Field Name="albedo" Type="vec4"/>            
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

      layout(location = 0) out vec3 normalVS;

      void main()
      {
        mat4 mv = camera.worldToView * model.transform;		
        normalVS =  normalize( mv * vec4(aNormal,0.0) ).xyz;
        mat4 mvp = camera.projection * mv;
        gl_Position =  mvp * vec4(aPosition,1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec3 normalVS;
            
      layout(location = 0) out vec4 color;
      layout(location = 1) out vec4 normal;
                        
      void main()
      {
        color = globals.albedo;
        normal = vec4(normalVS, gl_FragCoord.z);
      }			
    </FragmentShader>
  </Pass>
</Shader>