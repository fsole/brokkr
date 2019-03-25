<Shader Name="ssao" Version="440 core" >

    <Resources>    
        <Resource Name="globals" Type="uniform_buffer" Shared="no">						
            <Field Name="radius" Type="float" />
            <Field Name="bias" Type="float" />
            <Field Name="sampleCount" Type="int" />
        </Resource>
        
        <Resource Name="ssaoKernel" Type="storage_buffer" Shared="yes">
            <Field Name="data" Type="vec4" Count="64" />
        </Resource>
        
        <Resource Name="normalDepthTexture" Type="texture2D" />
        <Resource Name="ssaoNoise" Type="texture2D" />
    </Resources>


    <Pass Name="blit">
        <ZWrite Value="Off" />
        <ZTest Value="Always" />
        
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
            layout(location = 0) out float color;
            
            vec3 viewSpacePositionFromDepth(vec2 uv, float depth)
            {
                vec3 clipSpacePosition = vec3(uv* 2.0 - 1.0, depth);
                vec4 viewSpacePosition = camera.projectionInverse * vec4(clipSpacePosition,1.0);
                return(viewSpacePosition.xyz / viewSpacePosition.w);
            }
  
            vec2 screenSpaceFromViewSpace( vec3 position )
            {
                vec4 result = vec4(position, 1.0);
                result = camera.projection * result;    // from view to clip-space
                result /= result.w;                     // perspective divide
                result.xyz  = result.xyz * 0.5 + 0.5;   // transform to range 0.0 - 1.0  
                return result.xy;
            }
            
            void main()
            {
                vec4 normalDepth = texture(normalDepthTexture, uv);
                vec3 normal = normalize( normalDepth.rgb );
                vec3 positionVS = viewSpacePositionFromDepth( uv, normalDepth.w );
                
                vec2 noiseScale = vec2(textureSize(normalDepthTexture, 0) / vec2(4.0,4.0));
                vec3 randomVec = texture(ssaoNoise, uv * noiseScale).xyz;
                
                vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
                vec3 bitangent = cross(normal, tangent);
                mat3 TBN = mat3(tangent, bitangent, normal);
                
                float occlusion = 0.0;
                float radius = globals.radius;
                float bias = globals.bias;
                
                for(int i = 0; i &lt; globals.sampleCount; ++i)
                {
                    vec3 samplePosition = TBN * ssaoKernel.data[i].xyz; // From tangent to view-space
                    samplePosition = positionVS + samplePosition * radius; 
                    
                    vec2 sampleUV = screenSpaceFromViewSpace(samplePosition);
                    float sampleDepth = viewSpacePositionFromDepth( sampleUV, texture(normalDepthTexture, sampleUV).w ).z;
                    
                    float rangeCheck = smoothstep(0.0, 1.0, radius / abs(positionVS.z - sampleDepth));
                    occlusion += (sampleDepth &gt;= samplePosition.z + bias ? 1.0 : 0.0) * rangeCheck;
                }
                
                color = 1.0 - (occlusion /  float(globals.sampleCount) );
            }			
        </FragmentShader>

    </Pass>
	
</Shader>