<Shader Name="diffuse" Version="440 core" >

  <Resources>

    <Resource Name="globals" Type="uniform_buffer" Shared="yes">
      <Field Name="lightDirection" Type="vec4"/>
      <Field Name="fogPlane"       Type="vec4" />
      <Field Name="fogParameters"  Type="vec4" />
    </Resource>

    <Resource Name="properties" Type="uniform_buffer" Shared="no">
      <Field Name="kd" Type="vec4"/>
      <Field Name="ks" Type="vec4"/>
    </Resource>

    <Resource Name = "MainTexture" Type = "texture2D" />
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
      
      vec3 applyHalfSpaceFog(vec3 P, vec3 C, vec4 fogPlane, vec4 fogParameters, vec3 color)
      {
        //Displace point P along fog plane normal to account for plane offset
        vec3 F = normalize(fogPlane.xyz);
        P = P - F * fogPlane.w;

        vec3 V = C - P;
        float FdotV = dot(F, V);
        float FdotC = dot(F, C);
        float FdotP = dot(F, P);
        
        float k = mix(1.0, 0.0, step(0.0, FdotC));  //k=0 if FdotC is less than 0, 1 otherwise
        float c1 = k * (FdotP + FdotC);
        float c2 = min( (1 - 2 * k) * (FdotP), 0.0);
        float g = -fogParameters.a * length(V) * (c1 - c2 * c2 / abs(FdotV));
        float f = clamp(exp2(-g), 0, 1);

        return color.rgb * f + fogParameters.rgb * (1.0 - f);
      }

      void main()
      {
        //Lighting
        vec4 diffuseTex = texture(MainTexture, uv);
        color = (max( 0, dot(normalWS, globals.lightDirection.xyz)) + 0.2) * properties.kd * diffuseTex;

        //Fog
        color.rgb = applyHalfSpaceFog(positionWS, cameraPositionWS, globals.fogPlane, globals.fogParameters, color.rgb);
      }			
    </FragmentShader>
  </Pass>
</Shader>