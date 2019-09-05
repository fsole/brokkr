<Shader Name="diffuse" Version="440 core" >

  <Resources>

    <Resource Name="globals" Type="uniform_buffer" Shared="yes">
      <Field Name="light" Type="vec4"/>
      <Field Name="fogPlane"       Type="vec4" />
      <Field Name="fogParameters"  Type="vec4" />
      <Field Name="worldToLightClipSpace"  Type="mat4" />
      <Field Name="shadowMapSize"  Type="int" />
    </Resource>

    <Resource Name="properties" Type="uniform_buffer" Shared="no">
      <Field Name="kd" Type="vec4"/>
      <Field Name="ks" Type="vec4"/>
      <Field Name="shininess" Type="float" />
    </Resource>

    <Resource Name="diffuseTexture" Type="texture2D" />
    <Resource Name="opacityTexture" Type="texture2D" />
    <Resource Name="normalTexture" Type="texture2D" />
    <Resource Name="specularTexture" Type="texture2D" />
    <Resource Name="shadowMap" Type="texture2D" />
    

  </Resources>

  <Pass Name="OpaquePass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="Back"/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;
      layout(location = 3) in vec3 aTangent;

      layout(location = 0) out vec2 uv;
      layout(location = 1) out vec3 positionWS;
      layout(location = 2) out vec3 cameraPositionWS;
      layout(location = 3) out vec3 lightTS;
      layout(location = 4) out vec3 viewTS;

      void main()
      {
        positionWS = (model.transform * vec4(aPosition, 1.0)).xyz;
        cameraPositionWS = camera.viewToWorld[3].xyz;

        //Compute matrix that transforms from world space to tangent space
        vec3 tangentWS = normalize((model.transform * vec4(aTangent, 0.0)).xyz);
        vec3 normalWS = normalize((model.transform * vec4(aNormal, 0.0)).xyz);
        //Gram-Schimdt re-orthogonalization
        tangentWS = normalize(tangentWS - dot(tangentWS, normalWS) * normalWS);
        vec3 bitangentWS = cross(normalWS, tangentWS);
        mat3 TBN = transpose(mat3(tangentWS,bitangentWS, normalWS));
        lightTS = TBN * normalize(globals.light.xyz);
        viewTS = TBN * normalize(cameraPositionWS - positionWS);

        uv = aUV;
        gl_Position = (camera.viewProjection * model.transform) * vec4(aPosition, 1.0);
      }
    </VertexShader>
    
    <FragmentShader>               
      layout(location = 0) in vec2 uv;
      layout(location = 1) in vec3 positionWS;
      layout(location = 2) in vec3 cameraPositionWS;
      layout(location = 3) in vec3 lightTS;
      layout(location = 4) in vec3 viewTS;

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
        float opacity = texture(opacityTexture, uv).r;
        if (opacity &lt; 0.01) discard;

        vec3 normalTS = texture(normalTexture, uv).rgb;
        normalTS = normalize(normalTS * 2.0 - 1.0);
        
        //Diffuse
        float NdotL = max(0, dot(normalTS, lightTS));
        vec3 diffuseColor = texture(diffuseTexture, uv).rgb *  properties.kd.rgb;
        vec3 ambient = vec3(0.1) * diffuseColor;
        vec3 diffuse = NdotL * diffuseColor;
        
        //Specular
        float specularStrength = length( texture(specularTexture, uv).rgb );
        if (opacity &lt; 0.01) discard;
        vec3 halfVectorTS = normalize(viewTS + lightTS);
        float NdotH = max(0, dot(normalTS, halfVectorTS));
        vec3 specular = vec3(pow(NdotH, properties.shininess/2.0)) * NdotL;
        specular *= specularStrength;

        //Shadow
        vec4 postionInLigthClipSpace = globals.worldToLightClipSpace * vec4(positionWS, 1.0);
        postionInLigthClipSpace.xyz /= postionInLigthClipSpace.w;
        postionInLigthClipSpace.xy = 0.5 * postionInLigthClipSpace.xy + 0.5;
        ivec2 shadowMapUV = ivec2(postionInLigthClipSpace.xy * vec2(globals.shadowMapSize));        
        float bias = 0.002;        
        float attenuation = step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(0, 0), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(1, 0), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(-1, 0), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(0, 1), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(0, -1), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(1, 1), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(-1, 1), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(-1, -1), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation += step(0.5, float((texelFetch(shadowMap, shadowMapUV + ivec2(1, -1), 0).r + bias) &gt; postionInLigthClipSpace.z));
        attenuation /= 9.0;

        vec3 colorLinear = ( diffuse + specular) * globals.light.w * attenuation + ambient * globals.light.w;

        //Fog
        colorLinear = applyHalfSpaceFog(positionWS, cameraPositionWS, globals.fogPlane, globals.fogParameters, colorLinear);

        //Gamma
        color = vec4(pow(colorLinear, vec3(1.0 / 2.2)), 1.0);
      }			
    </FragmentShader>
  </Pass>

<Pass Name="DepthPass">
    <ZWrite Value="On"/>
    <ZTest Value="LEqual"/>
    <Cull Value="Back"/>

    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec3 aNormal;
      layout(location = 2) in vec2 aUV;
      layout(location = 3) in vec3 aTangent;

      void main()
      {
        gl_Position = (camera.viewProjection * model.transform) * vec4(aPosition, 1.0);
      }
    </VertexShader>
    
    <FragmentShader>      
      layout(location = 0) out vec4 color;
      void main()
      { 
        color = vec4(gl_FragCoord.z, 0, 0, 0);
      }			
    </FragmentShader>
  </Pass>
</Shader>