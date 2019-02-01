

<Shader Name="simple" Version="440 core" >

	<Resources>
		<Resource Name="globals" Type="uniform_buffer" Shared="no">
			<Field Name="diffuseColor" Type="vec4" />
			<Field Name="specularColor" Type="vec4" />
			<Field Name="shininess" Type="float" />
		</Resource>
		<Resource Name="lights" Type="storage_buffer" Shared="yes"> 			
			<Field Name="count" Type="int" />
			<Field Name="intensity" Type="float" />
			<Field Name="data" Type="compound_type" Count="">
				<Field Name="position" Type="vec4" />
				<Field Name="color" Type="vec3" />
				<Field Name="radius" Type="float" />
			</Field>
		</Resource>
	</Resources>
	
	
	<Pass Name="OpaquePass">
		<ZWrite Value="On" />
		<ZTest Value="LEqual" />
		<Cull Value="Back" />
		<Blend Target="0" SrcColor="SrcAlpha" DstColor="OneMinusSrcAlpha" SrcAlpha="" DstAlpha=""/>
				
		
		<VertexShader>
			layout(location = 0) in vec3 aPosition;
			layout(location = 1) in vec3 aNormal;
			layout(location = 2) in vec2 aUV;
			
			layout(location = 0) out vec4 positionVS;
			layout(location = 1) out vec3 normalVS;
			void main()
			{
				mat4 mv = camera.worldToView * model.transform;
				positionVS =  mv * vec4(aPosition,1.0);				
				normalVS =  normalize( mv * vec4(aNormal,0.0) ).xyz;				
				mat4 mvp = camera.projection * camera.worldToView * model.transform;
				gl_Position =  mvp * vec4(aPosition,1.0);
			}
		</VertexShader>
		
		<FragmentShader>
		
			vec3 applyLight(vec4 position, vec3 N, vec3 V, vec3 kd, vec3 ks, float shininess, vec4 lightPosition,  vec3 lightColor, float lightRadius)
			{
				vec3 pointToLight = (lightPosition - position ).xyz;
				float lightDistance = length(pointToLight);
				vec3 L = pointToLight / lightDistance;
				
				float NdotL = max( dot(N,L), 0.0 );
				vec3 diffuse = kd * NdotL;
				
				vec3 R = normalize(-reflect(L,N));
				vec3 specular = ks * pow(max(dot(R,V),0.0), shininess );
				
				float attenuation = pow(1.0 - clamp( lightDistance / lightRadius, 0.0, 1.0), 2);
				vec3 lightRadiance = lightColor * attenuation;
				
				return (diffuse + specular) * lightRadiance;
			}
			
			layout(location = 0) in vec4 positionVS;
			layout(location = 1) in vec3 normalVS;
			
			layout(location = 0) out vec4 color;
			void main()
			{
				vec3 V = normalize(-positionVS.xyz);
				vec3 c = vec3(0,0,0);
				for( int i = 0; i &lt; lights.count; ++i )
				{
					vec4 lightVS = camera.worldToView * lights.data[i].position;
					c += applyLight( positionVS, normalVS, V, globals.diffuseColor.rgb, globals.specularColor.rgb, globals.shininess, lightVS, lights.data[i].color, lights.data[i].radius ) * lights.intensity;
				}
				
				color = vec4( c, 1.0);
			}			
		</FragmentShader>
	
	</Pass>
	
</Shader>