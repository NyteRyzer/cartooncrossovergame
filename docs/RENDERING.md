# Rendering: Cel-Shaded Multiverse

Goal: every hero keeps their show's line work while sharing one Lumen-lit world.
This is done in two post-process passes plus per-character stencil tagging.

```
Lumen-lit scene ──► PP_ToonLighting (band the light, keep its hue)
                 ──► PP_ToonOutline  (Sobel depth+normal edges, style per universe via stencil)
```

## Pieces in the repo

| Piece | Where |
|---|---|
| HLSL library | `Plugins/ToonRender/Shaders/Private/ToonCommon.ush` (virtual path `/Plugin/ToonRender/Private/ToonCommon.ush`) |
| Shader dir registration | `Plugins/ToonRender/Source/ToonRenderShaders` (loads at PostConfigInit) |
| Stencil tagging | `UToonRenderLibrary::TagComponentWithUniverse`, called by `AToonCharacterBase::ApplyUniverseStencil` |
| Renderer config | `Config/DefaultEngine.ini`: `r.CustomDepth=3`, Lumen on, auto-exposure off |

Stencil value = `EToonUniverse + 1`. 0 = environment (falls back to the Nexus style).
The Omnitrix re-applies the stencil after every mesh swap.

## Building the materials in the editor

The materials are `.uasset` files, so they have to be authored in the editor. Each one is
a thin wrapper around a Custom node.

### `PP_ToonLighting` (Post Process material, Blendable Location: *Before Tonemapping*)

1. Material Domain = Post Process.
2. SceneTexture:PostProcessInput0 → `SceneColor` (Color)
3. SceneTexture:DiffuseColor → `Diffuse`
4. Scalar params `Bands` (3), `Softness` (0.15), `ShadowFloor` (0.35)
5. Custom node, output Float3, Include File Paths `/Plugin/ToonRender/Private/ToonCommon.ush`:
   ```hlsl
   return ToonQuantizeLighting(SceneColor, Diffuse, Bands, Softness, ShadowFloor);
   ```
6. → Emissive Color.

### `PP_ToonOutline` (Post Process, *Before Tonemapping*, after `PP_ToonLighting` via Blendable Priority)

Inputs: `UV` (ScreenPosition → ViewportUV), `TexelSize` (SceneTexelSize), `DepthSensitivity` (scalar, 40),
`Time` (Time node), `StyleLUT` (Texture Object param `T_UniverseLineStyles`), `Base` (PostProcessInput0).

```hlsl
float Darkness;
float Line = ToonOutline(UV, TexelSize, DepthSensitivity, Time, StyleLUT, StyleLUTSampler, Darkness);
return lerp(Base, Base * (1.0 - Darkness), Line);
```

Add SceneTexture:CustomStencil and SceneTexture:WorldNormal nodes to the graph (unconnected is fine)
so the material compiler makes those textures available to `SceneTextureLookup`.

### `T_UniverseLineStyles` (9×1, Nearest filtering, sRGB off, no mips)

One texel per universe in `EToonUniverse` order. Channels:
R = thickness/8 px, G = darkness, B = normal sensitivity, A = hand-drawn wobble.

| # | Universe | Look to match | Starting RGBA |
|---|---|---|---|
| 0 | Nexus / environment | thin, soft | 0.15, 0.6, 0.3, 0.0 |
| 1 | Retroville | CG toon, thin clean outline | 0.15, 0.8, 0.2, 0.0 |
| 2 | Dimmsdale | bold uniform black | 0.35, 1.0, 0.5, 0.0 |
| 3 | Amity Park | heavy angular ink | 0.45, 1.0, 0.7, 0.1 |
| 4 | Chinatown | medium, brushy | 0.30, 0.9, 0.5, 0.4 |
| 5 | Tremorton | crisp retro mechanical | 0.30, 1.0, 0.8, 0.0 |
| 6 | Miracle City | thick, wobbly Flash style | 0.40, 1.0, 0.5, 0.6 |
| 7 | Bellwood | medium, sharp | 0.25, 0.9, 0.6, 0.0 |
| 8 | Air Temple | thin, painterly | 0.12, 0.7, 0.3, 0.2 |

Tune these against show reference. Retroville is the odd one out: the show is CG, so it gets a
light line and leans on the banded lighting.

### Post Process Volume

Put an unbound PPV in every level with both materials in Blendables, Exposure Min = Max = 1,
and Lumen settings left at default. The greybox builder doesn't place it yet: add it by hand once
the materials exist.

## Next rendering work

- Per-character toon ramps (material function `MF_ToonRamp`) for each show's shadow colour, instead of only the global post band.
- Inverted-hull outlines on hero meshes for thicker inner lines than screen-space Sobel can give.
- Make sure the outline pass ignores translucency and VFX (stencil bit or a separate translucency pass).
