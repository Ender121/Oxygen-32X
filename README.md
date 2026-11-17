# Oxygen-32X

Fork of the Oxygen Engine (originally the engine behind "Sonic 3 - Angel Island Revisited" by Eukaryot), with all Sonic 3 A.I.R.-specific code, scripts and data removed. What remains is the base engine, ready to be used as a foundation for other projects.

Upstream project: https://github.com/Eukaryot/sonic3air

### Disclaimer

Sonic the Hedgehog is a trademark of SEGA. This is a non-profit fan project, not affiliated in any way with SEGA or Sonic Team.
Any commercial use of this project without SEGA's explicit consent is strictly prohibited.


## Repository overview

* The larger external dependencies (namely SDL2, Ogg/Vorbis, zlib) inside the "framework" directory. These are copies of the respective open source projects, with a few custom changes applied where needed - see the "how-to-build.txt" files in there for details.
* The librmx libraries that together with the external dependencies build a foundation for the engine. This primarily consists of rmxbase, a collection of helper and utility classes, and rmxmedia, which is providing a basic game framework built on top of SDL2 & OpenGL.
* Lemonscript language library, with compiler and runtime environment for script execution.
* Oxygen Engine in "Oxygen/oxygenengine": input, graphics, audio handling, and all the other game application stuff, as well as a simulation environment built around lemonscript that emulates aspects of Sega Genesis hardware. It builds as its own application (OxygenApp).
* Oxygen server in "Oxygen/oxygenserver".


## How to build

The Windows (Visual Studio) solution for the engine is in "Oxygen/oxygenengine/build/_vstudio".


## External dependencies

External libraries and code used in this project:
* SDL2 - in "framework/external/sdl"
* libogg & libvorbis - in "framework/external/ogg-vorbis"
* zlib incl. minizip - in "framework/external/zlib"
* libcurl - in "framework/external/curl"
* Dear ImGui - in "framework/external/imgui"
* jsoncpp - in "librmx/source/rmxbase/jsoncpp"
* GLEW - in "librmx/source/rmxmedia/glew"
* Sound chip emulation related code from Genesis Plus GX - in "Oxygen/oxygenengine/source/oxygen/simulation/sound"
* xBRZ upscaler shader code - in "Oxygen/oxygenengine/data/shader"
* Hqx upscaler shader code & data files - in "Oxygen/oxygenengine/data/shader"


## License

See the LICENSE file. The librmx, lemonscript and Oxygen Engine code is by Eukaryot; see upstream for the full contributor list.
