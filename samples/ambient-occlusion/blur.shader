<Shader Name="blur" Version="440 core" >

  <Resources>
    <Resource Name="MainTexture" Type="texture2D"/>
    <Resource Name="sceneColorTexture" Type="texture2D"/>
  </Resources>
  
  <Pass Name="blit">
    <ZWrite Value="Off"/>
    <ZTest Value="Always"/>
        
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
        vec2 texelSize = 1.0 / vec2(textureSize(MainTexture, 0));
        float occlusion = 0.0;                
        for (int x = -2; x &lt; 2; ++x) 
        {
          for (int y = -2; y &lt; 2; ++y) 
          {
            occlusion += texture(MainTexture, uv + vec2(x, y)*texelSize ).r;
          }
        }
        occlusion /= 16.0;
        color = texture(sceneColorTexture, uv) * occlusion;
      }			
    </FragmentShader>
  </Pass>	
</Shader>