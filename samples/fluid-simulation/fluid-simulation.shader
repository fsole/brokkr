<Shader Name="fluidSimulation" Version="440 core" >

  <Resources>
    <Resource Name="globals" Type="storage_buffer">
      <Field Name="deltaTime" Type="float"/>
      <Field Name="particlesToEmit" Type="int"/>
      <Field Name="gravity" Type="float"/>
      <Field Name="initialVelocity" Type="float"/>
      <Field Name="emissionVolume" Type="vec3"/>
      <Field Name="maxParticleCount" Type="int"/>
      <Field Name="emissionDirection" Type="vec4"/>
      <Field Name="boundaries" Type="vec4" Count="6"/>      
      <Field Name="viscosityCoefficient" Type="float"/>
      <Field Name="pressureCoefficient" Type="float"/>
      <Field Name="smoothingRadius" Type="float"/>
      <Field Name="referenceDensity" Type="float"/>
      <Field Name="particleMass" Type="float"/>
    </Resource>
        
    <Resource Name="particles" Type="storage_buffer" Shared="yes">
      <Field Name="data" Type="compound_type" Count="">
        <Field Name="position" Type="vec3"/>
        <Field Name="scale" Type="float"/>
        <Field Name="color" Type="vec4"/>
        <Field Name="angle" Type="vec3"/>
      </Field>
    </Resource>
        
    <Resource Name="particlesState" Type="storage_buffer" Shared="yes">
      <Field Name="data" Type="compound_type" Count="">
        <Field Name="velocity" Type="vec3"/>
        <Field Name="age" Type="float"/>
        <Field Name="density" Type="float"/>
        <Field Name="pressure" Type="float"/>
        <Field Name="mass" Type="float"/>
      </Field>
    </Resource>                
  </Resources>
  
  <ComputeShader Name="computeDensity" LocalSizeX="64" LocalSizeY="1" >
    const float PI = 3.1415926;
    void main()
    {
      float h = globals.smoothingRadius; //smoothing radius parameter
      float h9 = pow(h, 9);
      float h2 = h*h;
      float poly6Coefficient = (315.0f / (64.0f * PI * h9));
      
      uint particleIndex = gl_GlobalInvocationID.x;    
      if( particleIndex &gt; globals.maxParticleCount )
      {
        return;      
      }
            
      particlesState.data[particleIndex].density = 0;
      for( int i = 0; i &lt; globals.maxParticleCount; ++i )
      {
        if( particlesState.data[i].age != -1 )
        {
          vec3 diff = particles.data[particleIndex].position - particles.data[i].position;
          float r2 = dot(diff, diff);
          if(r2 &lt; h2)
          {
            const float W = poly6Coefficient * pow(h2 - r2, 3);
            particlesState.data[particleIndex].density += particlesState.data[particleIndex].mass * W;
          }
        }
      }

      particlesState.data[particleIndex].density = max(globals.referenceDensity, particlesState.data[particleIndex].density);
      particlesState.data[particleIndex].pressure = globals.pressureCoefficient * (particlesState.data[particleIndex].density - globals.referenceDensity);
    }
  </ComputeShader>

  <ComputeShader Name="updateParticles" LocalSizeX="64" LocalSizeY="1" >
    uint rng_state = 0;
    const float PI = 3.1415926;
    void initRand()
    {
      uint seed = gl_GlobalInvocationID.x + uint( 1000.0 * fract( globals.deltaTime) );
      seed = (seed ^ 61) ^ (seed &gt;&gt; 16);
      seed *= 9;
      seed = seed ^ (seed &gt;&gt; 4);
      seed *= 0x27d4eb2d;
      seed = seed ^ (seed &gt;&gt; 15);
      rng_state = seed;
    }

    float rand()
    {
      rng_state ^= (rng_state &lt;&lt; 13);
      rng_state ^= (rng_state &gt;&gt; 17);
      rng_state ^= (rng_state &lt;&lt; 5);
      return float(rng_state) * (1.0 / 4294967296.0);
    }

    vec3 RandomPointInSphere()
    {
      float z = rand() * 2.0f - 1.0f;
      float t = rand() * 2.0f * PI;
      float r = sqrt(max(0.0, 1.0f - z*z));
      float x = r * cos(t);
      float y = r * sin(t);
      vec3 res = vec3(x,y,z);
      res *= pow(rand(), 1.0/3.0);
      return res;
    }
         
    void main()
    {
      initRand();
      uint particleIndex = gl_GlobalInvocationID.x;    
      if( particleIndex &gt; globals.maxParticleCount )
      {
        return;      
      }

      if( particlesState.data[particleIndex].age &lt; 0 )
      {      
        //Emit if required
        if( atomicAdd( globals.particlesToEmit, -1 ) &gt; 0 )
        {
          //Initialize particle
          particles.data[particleIndex].scale = 0.5;
          vec3 randPos = globals.emissionVolume * vec3( 2.0*rand() - 1.0,2.0*rand() - 1.0,2.0*rand() - 1.0 );
          particles.data[particleIndex].position = randPos;
          particles.data[particleIndex].angle = vec3( 2.0*rand() - 1.0,2.0*rand() - 1.0,2.0*rand() - 1.0 );        
          particles.data[particleIndex].color = vec4( rand(), rand(), rand(), 1.0 );

          particlesState.data[particleIndex].age = 0;
          vec3 emissionDirection = normalize(  normalize( globals.emissionDirection.xyz) + globals.emissionDirection.w * RandomPointInSphere() );
          particlesState.data[particleIndex].velocity = globals.initialVelocity * emissionDirection;
          particlesState.data[particleIndex].mass = globals.particleMass;
        }
        else
        {
          particles.data[particleIndex].scale = 0;
        }
      }
      else
      { 
        vec3 acceleration = vec3(0,0,0);
        float h = globals.smoothingRadius;
        float h2 = h*h;
        float h3 = h*h*h;      
        float spikyGradCoefficient = (-45 / (PI * pow(h,6.0)));
        float viscosityLaplacianCoefficient = 45.0/(PI*pow(h, 6.0f));
        float particlePressure = particlesState.data[particleIndex].pressure;
        float particleDensity = particlesState.data[particleIndex].density;

        if( particlesState.data[particleIndex].age &gt; 1.0 )
        {
          for( int i = 0; i &lt; globals.maxParticleCount; ++i )
          {
            if( particlesState.data[i].age != -1 && i != particleIndex)
            {          
              vec3 r = particles.data[particleIndex].position - particles.data[i].position;
              float distance = length( r );
              if( distance &lt; h )
              {
                //Acceleration due to pressure
                float diff = h - distance;
                float spiky = spikyGradCoefficient*diff*diff;
                float massRatio = particlesState.data[i].mass / particlesState.data[particleIndex].mass;
                float pterm = (particlePressure + particlesState.data[i].pressure) / (2*particleDensity*particlesState.data[i].density);
                acceleration -= massRatio*pterm*spiky*r;
                      
                //Acceleration due to viscosity
                float lap = viscosityLaplacianCoefficient*diff;
                vec3 vdiff = particlesState.data[i].velocity - particlesState.data[particleIndex].velocity;
                acceleration += globals.viscosityCoefficient*massRatio*(1/particlesState.data[i].density)*lap*vdiff;
              }
            }
          }
        }

        //Acceleration due to gravity
        acceleration.y -= globals.gravity;

        //Update particle position, velocity and age
        particlesState.data[particleIndex].age += globals.deltaTime;
        particlesState.data[particleIndex].velocity +=  acceleration * globals.deltaTime;
        particles.data[particleIndex].position +=  particlesState.data[particleIndex].velocity * globals.deltaTime;
              
        //Bounds check
        for( int i=0; i &lt; 6; ++i )
        { 
          vec4 bound = globals.boundaries[i];
          bound.xyz = normalize( bound.xyz );
          float distanceToPlane = dot( bound, vec4(particles.data[particleIndex].position, 1.0) );
          if( distanceToPlane &lt;= 0.0 )
          {
            particles.data[particleIndex].position -= bound.xyz*distanceToPlane;
            vec3 reflectionDirection = reflect( particlesState.data[particleIndex].velocity, bound.xyz );
            particlesState.data[particleIndex].velocity = reflectionDirection * 0.3;
          }
        }
      }  
    }
  </ComputeShader>
</Shader>