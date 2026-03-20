
how to run:
under the EXECUTABLE file there will be Project_Template.exe, run this to play the game, all the necessary files are included.

instructions on how to play:

This project lets the user move around a simple 3D environment as a character
(hecarim) from the video game League of Legends.

Controls
- WASD to move (when in 3rd person locked mode).
- Mouse to look around.
- TAB toggles between the default 3rd person camera and a free camera.
- Q collects a nearby minion when in range.
- The cursor is locked while playing and released on win/lose or when TAB toggles modes.

Objective
Collect minions by pressing Q. Collect 15 minions to win.
If the whisp reaches Hecarim, the crown is removed and you lose.

Version and OS:
- Visual Studio 2022, Windows 11

Architecture
- main.cpp sets up GLFW, GLAD and the window, creates the scene and runs the main loop.
- scenebasic_uniform.cpp has the Scene class: input, update, shadow pass, skybox, model rendering and particle systems.
- shaders/ contains vertex and fragment shaders (basic_uniform, sky_noise, particle, depth).
- media/ contains textures and OBJ models loaded at runtime.

What’s new

Skybox (shader/sky_noise.vert + shader/sky_noise.frag)
- Procedural sky rendered from a cube-geometry:
  - Vertex shader uses ViewNoTrans + Projection and writes gl_Position = pos.xyww to keep the sky visually at infinity.
  - Fragment shader uses hashed grid noise, FBM (fractal brownian motion), and layered effects.
  - Visuals:
  - Horizon color blend into deep space.
  - Star field generated from noise.
  - Multi-layer nebula using FBM layers and a small colour palette blending to produce a nebula effect.
  - Uniforms used from scenebasic_uniform.cpp: ViewNoTrans, Projection, Time.

Particle system (shader/particle.vert + shader/particle.frag)
- Instanced particles implemented:
  - Per instance attributes: InstancePosSize (vec4: xyz = world pos, w = radius) and InstanceLifePad (vec4: x = life 0..1, y = type id, z/w reserved).
  - MAX_PARTICLES = 12000, instance buffered every frame.
  - particle.vert gets CameraRight and CameraUp which are transformed to view space to produce  positions so particles always face the camera.
  - Projection and view matrices are provided per frame.

- Particle types implemented in particle.frag:
  - TypeID 0 = crown / fire-like (orange > red > grey > black over life).
  - TypeID 1 = explosion (brighter fire profile).
  - TypeID 2 = whisp trail (dark bluish → grey).

- Per-particle shading:
  - Particles are treated as spherical surfaces on the quad, produce normals for simple diffuse + specular lighting in view-space.
  - Alpha falloff at the edges produce soft particles.


Materials, normal maps, shadows, fog (shader/basic_uniform.vert + shader/basic_uniform.frag + scenebasic_uniform.cpp)
- Materials & normal mapping:
  - basic_uniform.vert has per-vertex tangent / bitangent and a NormalMatrix so the fragment shader can construct a 3x3 matrix.
  - basic_uniform.frag samples NormalMap and transforms the normal with the TBN to get the correct lighting detail from normal maps.

 
- Shadows
  - Shadow map generation:
    - scenebasic_uniform.cpp creates one depth texture (shadowTexs) and an FBO (shadowFBOs) per light (8).
    - Depth textures are created with internal format GL_DEPTH_COMPONENT24 and resolution.
    - The code performs a depth pass per light. For each light, it constructs a LightSpace matrix (lightProj * lightView)
      and renders scene geometry with a depth only shader (depthProg) to create that light's depth texture.
    - The light view for each shadow map is produced by a Frustum helper
      and setPerspective(60.0f, 1.0f, 1.0f, 60.0f)). The result is stored in lightSpaces.
    
  - Shadow sampling in the lighting pass:
    - basic_uniform.frag has arrays: sampler2D ShadowMap[NUM_LIGHTS] and mat4 LightSpace[NUM_LIGHTS].
    - For each light, computeShadowForLight(int idx, vec4 worldPos, vec3 n, vec3 viewPos) does:
      - Transform worldPos by LightSpace[idx] > lightCoord. If lightCoord.w <= 0.0, it gets treated as unshadowed.
      - Map to texture UVs: uv = projCoords.xy * 0.5 + 0.5. Compute currentDepth = projCoords.z * 0.5 + 0.5.
      - If uv is outside [0,1], the fragment unshadowed (value = 1.0).

Gameplay / scene behavior additions (scenebasic_uniform.cpp)
- Spotlights:
  - NUM_LIGHTS = 8 spotlights, lights move with randomized targets, speeds and intervals. Lights drive the Blinn-Phong shading and shadow mapping.
- Shadow pass:
  - A depth-only pass renders scene geometry into each light's shadow map.
  - Uses a small depth shader (depthProg) and a light frustum helper to compute LightSpace matrices.
- Particles and Whisp:
  - initParticleSystem() sets up VAO/VBO/instanced VBO, creates a 1D texture.
  - spawnParticleRing, spawnParticleExplosion, updateParticles have particle lifecycle, spawn rates, physics and behaviors.
  - Whisp follows Hecarim and spawns a trail of particles; if the whisp contacts the player the crown is disabled, the whisp dies and the game is lost.

- Textures used by the scene:
  - media/lambert4.png (main character diffuse)
  - media/ground.jpg (ground)
  - media/RedMinion.png (minion texture)


  expensive parts of this project are the shadow mapping and particle systems, having a depth map per light and running a 3x3 PCF
  per light in the shader is a large workeload on the GPU. The consequence of this is there is a high  VRAM usage.


  I got the idea for this game from my favourite game, League of Legends and I wanted to create a simple 3D game
  where the player can play as my favourite character, Hecarim. And in the game the player has to collect minions
  (among many other things) and just doing that alone felt not enough so I added the whisp to create a bit more a fun
  sense of urgency to the game, I also added the skybox and particle system to make the game look a bit nicer because
  in coursework 1 it felt a bit out of place and bland. I did try and impliment a PBR system but I got very confused,
  if I had more time I would have liked to try and impliment it properly but I am still proud of what I have achieved
  in this coursework, especially the skybox which turned out quite nicely.





youtube video:


github repo:
https://github.com/AGoslin1/COMP3015-CW1

gen AI statement:
all code is implemeted by me, I have not used any gen AI tools to write this code, I have only used gen AI
in a teaching way to understand certain features.