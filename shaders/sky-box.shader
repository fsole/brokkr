<Shader Name="internal/sky-box" Version="440 core" >

    <Resources>		
        <Resource Name="CubeMap" Type="textureCube" />
    </Resources>
	
	
    <Pass Name="blit">
        <ZWrite Value="Off" />
        <ZTest Value="LEqual" />
		
    <VertexShader>
            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec2 aUV;

            layout(location = 0) out vec3 uvCubemap;
            void main()
            {
                gl_Position = vec4(aPosition,1.0);
                gl_Position.z = 1.0;
                
                mat4 viewNoTranslation = camera.worldToView;
                viewNoTranslation[3][0] = viewNoTranslation[3][1] = viewNoTranslation[3][2] = 0.0;
                mat4 skyBoxTransform = inverse( camera.projection * viewNoTranslation );
                uvCubemap = (skyBoxTransform * gl_Position ).xyz;
            }
            </VertexShader>
		
            <FragmentShader>			
                layout(location = 0) in vec3 uvCubemap;
                layout(location = 0) out vec4 color;
                void main()
                {
                    color = textureLod(CubeMap,uvCubemap, 0);
                }			
            </FragmentShader>	
    </Pass>
</Shader>