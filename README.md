# ARCHITECT: PBR maps from just an albedo?!?!

It's actually not that complicated. I wrote this a while back for a retro style game that I was helping with. Because this was written quickly and was only used by me, there are a few quirks that you should know.

- Launch the editor utility widget and dock it somewhere.
- With an albedo map selected (must have the suffix "_Albedo"), just hit the generate button.
- It will create aa Normal map and a pack texture with the suffix "_ORDM".
- The ORDM texture is the Ambient Occlusion, Roughness, Displacement (No algorithm is actually implemented for this, so you would need to create one), and Metallic.

The result can give you some interesting looking materials that can appear suprisingly convincing. Keep in mind however, these maps are obviously approximations and not physically accurate, so results may vary based on the material that you are trying to emulate.
