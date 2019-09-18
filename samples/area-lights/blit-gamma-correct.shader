<Shader Name="blitGamma" Version="440 core" >

  <Resources>
    <Resource Name="MainTexture" Type="texture2D"/>
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
        color = vec4(pow(texture(MainTexture, uv).rgb, vec3(1.0 / 2.2)), 1.0);
      }			
    </FragmentShader>
  </Pass>	
</Shader>