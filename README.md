# The Force Engine (TFE)
[Roadmap](Roadmap.md)

The Force Engine is a project to reverse engineer and rebuild the Jedi Engine for modern systems and the games that used that engine - **Dark Forces** and **Outlaws**. The project includes modern, built-in tools, such as a level editor and makes it easy to play **Dark Forces** and **Outlaws** on modern systems as well as the many community **mods** designed to work with the original games.

Playing Dark Forces or Outlaws using the Force Engine adds ease of use and modern features such as higher resolutions and modern control schemes such as mouse-look. Using the built-in tools allows for easier modding with more modern UI, greater flexibility and the ability to use enhancements made to the Jedi Engine for Outlaws in custom Dark Forces levels - such as slopes, stacked sectors, per-sector color maps and more.

The project is focused on accuracy - by using reverse engineering techniques to reconstruct the original code and algorithms - the Force Engine is designed to be extremely accurate, a complete replacement for the original executables. The engine supports three feature templates to make it easier to tune the experience:
* **Classic** - `a recreation of the original software renderer, controls and gameplay - providing the original experience as close as possible while still properly supporting modern systems.`
* **Retro** - `close to the original experience while being enhanced with modern controls and high resolution rendering.`
* **Modern** - `modern enhancements such as proper perspective rendering, enhanced texture filtering, mipmapping, widescreen and more.`

From these general templates, settings can be fine tuned. One example is "Classic" settings with higher resolutions or mouse look. Or using the perspective renderer but sticking to 320x200. Both the "Classic" and Perspective renderers support pure software rendering and gpu based rendering, though some features - such as enhanced texture filtering - are only available using gpu rendering.
