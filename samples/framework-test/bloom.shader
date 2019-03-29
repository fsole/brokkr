<Shader Name="bloom" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="uniform_buffer" Shared="no">						
      <Field Name="bloomTreshold" Type="float" />
    </Resource>        
    <Resource Name="MainTexture" Type="texture2D" />		
  </Resources>

  <Pass Name="extractBrightPixels">
    <ZWrite Value="Off"/>
    <ZTest Value="Off"/>
        
    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec2 aUV;
            
      layout(location = 0) out vec2 uv;
      void main()
      {
        gl_Position = vec4(aPosition,1.0);
        uv = aUV;
      }
    </VertexShader>
        
    <FragmentShader>			
      layout(location = 0)in vec2 uv;
      layout(location = 0) out vec4 color;
      void main()
      {
        vec4 imageColor = texture(MainTexture, uv);
        float brightness = dot(imageColor.rgb, vec3(0.2126, 0.7152, 0.0722));
        if (brightness &gt; globals.bloomTreshold)
        {
          color = vec4(imageColor.rgb, 1.0);
        }
        else
        {
          color = vec4(0.0, 0.0, 0.0, 1.0);
        }
      }			
    </FragmentShader>	
  </Pass>

  <Pass Name="blurVertical">
    <ZWrite Value="Off"/>
    <ZTest Value="Off"/>
        
    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec2 aUV;
            
      layout(location = 0) out vec2 uv;
      void main()
      {
        gl_Position = vec4(aPosition,1.0);
        uv = aUV;
      }
    </VertexShader>
        
    <FragmentShader>							
      float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
      layout(location = 0)in vec2 uv;
      layout(location = 0) out vec4 color;

      void main()
      {			
        vec3 finalColor = vec3(0,0,0);
        vec2 tex_offset = 1.0 / textureSize(MainTexture, 0);
        finalColor = texture(MainTexture, uv).rgb * weight[0];
        for (int i=1; i &lt; 5; i++) 
        {
          finalColor += texture(MainTexture, uv + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
          finalColor += texture(MainTexture, uv - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }         
        color = vec4(finalColor,1.0);
      }			
    </FragmentShader>
  </Pass>

  <Pass Name="blurHorizontal">
    <ZWrite Value="Off"/>
    <ZTest Value="Off"/>
        
    <VertexShader>
      layout(location = 0) in vec3 aPosition;
      layout(location = 1) in vec2 aUV;
            
      layout(location = 0) out vec2 uv;
      void main()
      {
        gl_Position = vec4(aPosition,1.0);
        uv = aUV;
      }
    </VertexShader>
        
    <FragmentShader>
      float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
      layout(location = 0)in vec2 uv;
      layout(location = 0) out vec4 color;

      void main()
      {			
        vec3 finalColor = vec3(0,0,0);
        vec2 tex_offset = 1.0 / textureSize(MainTexture, 0);
        finalColor = texture(MainTexture, uv).rgb * weight[0];
        for (int i=1; i &lt; 5; i++) 
        {
          finalColor += texture(MainTexture, uv + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
          finalColor += texture(MainTexture, uv - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }                
        color = vec4(finalColor,1.0);
      }			
    </FragmentShader>	
  </Pass>
</Shader>