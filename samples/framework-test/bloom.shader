

<Shader Name="bloom" Version="440 core" >

	<Resources>
		<Resource Name="globals" Type="uniform_buffer" Shared="no">			
			<Field Name="imageSize" Type="vec4" />
			<Field Name="bloomTreshold" Type="float" />
			<Field Name="blurSigma" Type="float" />
		</Resource>
		
		<Resource Name="MainTexture" Type="texture2D" />		
	</Resources>
	
	
	<Pass Name="extractBrightColors">
		<ZWrite Value="Off" />
		<ZTest Value="Off" />
		
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
				if(brightness &gt; globals.bloomTreshold )
					color = vec4(imageColor.rgb, 1.0);
				else
					color = vec4(0.0, 0.0, 0.0, 1.0);
			}			
		</FragmentShader>	
	</Pass>
	
	<Pass Name="blur">
		<ZWrite Value="Off" />
		<ZTest Value="Off" />
		
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
			
			float normpdf(in float x, in float sigma)
			{
				return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
			}

			void main()
			{			
				const int mSize = 11;
				const int kSize = (mSize-1)/2;
				float kernel[mSize];
				vec3 finalColor = vec3(0.0);
				
				//create the 1-D kernel
				float sigma = globals.blurSigma;
				float Z = 0.0;
				for (int j = 0; j &lt;= kSize; ++j)
				{
					kernel[kSize+j] = kernel[kSize-j] = normpdf(float(j), sigma);
				}
				
				//get the normalization factor (as the gaussian has been clamped)
				for (int j = 0; j &lt;= mSize; ++j)
				{
					Z += kernel[j];
				}
				
				//read out the texels
				for (int i=-kSize; i &lt;= kSize; ++i)
				{
					for (int j=-kSize; j &lt;= kSize; ++j)
					{
						vec2 sampleUv = (gl_FragCoord.xy + vec2(float(i),float(j))) * globals.imageSize.zw;
						finalColor += kernel[kSize+j]*kernel[kSize+i]* texture(MainTexture, sampleUv ).rgb;
			
					}
				}
				
				color = vec4(finalColor,1.0);
			}			
		</FragmentShader>	
	</Pass>
</Shader>