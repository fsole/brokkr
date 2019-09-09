<Shader Name="particles" Version="440 core" >

  <Resources>
    <Resource Name="particles" Type="storage_buffer" Shared="yes">
      <Field Name="data" Type="compound_type" Count="">
        <Field Name="position" Type="vec3"/>
        <Field Name="scale" Type="float"/>
        <Field Name="color" Type="vec4"/>
        <Field Name="angle" Type="vec3"/>
      </Field>
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

      layout(location = 0) out vec4 color;

      mat3 rotationFromEuler( vec3 eulerAngles )
      {
        mat3 mx;
        float s = sin(eulerAngles.x);
        float c = cos(eulerAngles.x);
        mx[0] = vec3(c, s, 0.0);
        mx[1] = vec3(-s, c, 0.0);
        mx[2] = vec3(0.0, 0.0, 1.0);
        
        mat3 my;
        s = sin(eulerAngles.y);
        c = cos(eulerAngles.y);
        my[0] = vec3(c, 0.0, s);
        my[1] = vec3(0.0, 1.0, 0.0);
        my[2] = vec3(-s, 0.0, c);
        
        mat3 mz;
        s = sin(eulerAngles.z);
        c = cos(eulerAngles.z);		
        mz[0] = vec3(1.0, 0.0, 0.0);
        mz[1] = vec3(0.0, c, s);
        mz[2] = vec3(0.0, -s, c);
        
        return mz * my * mx;
      }
      
      void main()
      {
        color = particles.data[gl_InstanceIndex].color;
        mat3 rotation = rotationFromEuler(particles.data[gl_InstanceIndex].angle);
        vec3 localPosition = aPosition.xyz * rotation * particles.data[gl_InstanceIndex].scale + particles.data[gl_InstanceIndex].position;
        mat4 viewProjection = camera.projection * camera.worldToView;
        gl_Position = viewProjection * vec4(localPosition, 1.0);
      }
    </VertexShader>
    
    <FragmentShader>
      layout(location = 0) in vec4 color;
            
      layout(location = 0) out vec4 result;
                        
      void main()
      {
        result = color;
      }			
    </FragmentShader>
  </Pass>
</Shader>