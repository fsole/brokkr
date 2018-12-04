

<Shader Name="simple" Version="440 core" >

	<Resources>
		<Resource Name="globals" Type="uniform_buffer" Shared="no">
			<Field Name="color" Type="vec3" />
			<Field Name="intensity" Type="float" />
		</Resource>		
	</Resources>
	
	
	<Pass Name="OpaquePass">
		<ZWrite Value="On" />
		<ZTest Value="LEqual" />
		<Blend Target="0" SrcColor="SrcAlpha" DstColor="OneMinusSrcAlpha" SrcAlpha="" DstAlpha=""/>
				
		
		<VertexShader>
			layout(location = 0) in vec3 aPosition;
			layout(location = 1) in vec3 aNormal;
			layout(location = 2) in vec2 aUV;
			
			void main()
			{
				mat4 mvp = camera.projection * camera.worldToView * model.transform;
				gl_Position =  mvp * vec4(aPosition,1.0);
			}
		</VertexShader>
		
		<FragmentShader>			
			layout(location = 0) out vec4 color;
			void main()
			{
				color = vec4( globals.color * globals.intensity, 1.0);
			}			
		</FragmentShader>
	
	</Pass>
	
</Shader>