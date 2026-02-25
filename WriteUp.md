
instructions on how to play:

This project is has the user move around a simple 3D environment as a character
(hecarim) from the video game League of legends.

The user can move around with WASD and look around using the mouse,
there will be other characters that spawn around the map.

When hecarim comes close enough to one of the "minions" he can press Q to collect it,
once he has 15 minions you win the game.

The game has a dark ambient light with many spotlights spawning in and out
around the plane showing the user where the minions are.

the light uses a blinnphong shading model and the models are brighter when inside the
spotlights.

The user can also press TAB to change between the default 3rd person camera mode and
a free camera that can move independently so you can inspect the model better.

version and OS:
visual studio 2022, windows 11

how does it work:
the game uses a simple game loop that updates the position of the player and the minions,
it also checks for collisions between the player and the minions to see if the player can collect them.

it uses lighting system that spawns spotlights around the map to show the user where the minions are,
the spotlights use a blinnphong shading model to make the models brighter when inside the light.

it uses the basic_uniform frag and vert shaders to render the models and the lighting.
the game also has a skybox that uses a cubemap texture to render the sky.

main.cpp sets up GLFW, GLAD and the window then creates the Scene and runs the main loop.

scenebasic_uniform.cpp contains the scene class that handles input, update and render.

shaders contain vertex and fragment logic; the fragment shader accumulates lighting from the spotlights.

helper/ contains small utilities (texture loader, mesh loader, ect.).

media/ contains textures and obj models loaded at runtime.

youtube video:
https://youtu.be/rpy5L-Xr4f8

github repo:
https://github.com/AGoslin1/COMP3015-CW1