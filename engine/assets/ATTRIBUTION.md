# Third-party demo assets

These assets are kept separate from NK Engine's MIT-licensed source code. Their
presence here does not relicense them under the NK Engine license.

## Crytek Sponza

- Files: `models/sponza.obj`, `models/sponza.mtl` and the texture maps whose
  names begin with `sponza_` or `spnza_`, plus the related `background`,
  `chain_texture`, `lion` and `vase` maps referenced by the MTL.
- Original model: Frank Meinl, Crytek (2010).
- Tutorial source: [Kohi commit 6f5b979](https://github.com/travisvroman/kohi/commit/6f5b979bccd59e84afb2155440bdaf4e014ad3b5).
- Common archive attribution: [Morgan McGuire's Computer Graphics Archive](https://casual-effects.com/data).
- NK conversion: referenced TGA maps were converted to PNG and capped at
  512 pixels on their largest axis. Opacity maps are not included because the
  current material model has no opacity-map slot.

## Falcon wreck automobile

- Files: `models/falcon.obj`, `models/falcon.mtl` and
  `textures/falc_wreck_low_DefaultMaterial_*.png`.
- Tutorial source: [Kohi commit 6f5b979](https://github.com/travisvroman/kohi/commit/6f5b979bccd59e84afb2155440bdaf4e014ad3b5).
- The upstream snapshot does not include separate author or license metadata
  for this model. Do not assume that NK Engine's MIT license covers it; verify
  the asset's original terms before redistributing it outside this tutorial
  repository.
- NK conversion: the three 4096-pixel PNG maps were capped at 1024 pixels on
  their largest axis.

## Humus skybox

- Files: `textures/skybox_{r,l,u,d,f,b}.png`, ordered as
  +X, -X, +Y, -Y, +Z and -Z.
- Author: Emil Persson (Humus), <http://www.humus.name>.
- License: [Creative Commons Attribution 3.0](https://creativecommons.org/licenses/by/3.0/).
- Tutorial source: [Kohi commit ab99e89](https://github.com/travisvroman/kohi/commit/ab99e894b24a18598412c42c1545c892042e3dfa).
- NK conversion: the six upstream 2048x2048 JPEG files were decoded and
  stored losslessly as PNG because NK's image loader intentionally supports
  PNG only. The upstream JPEG SHA-256 hashes for `b,d,f,l,r,u` are
  `5c7f2787518affc762f800b7e27c63b227d4d958c3daa83ceb24c7d756d43500`,
  `a6b88055cf99db774349a9eb382c3ca8dccc219a1ce6ffb7e849459155fae14a`,
  `1df219023d436849e68d89c4524cc9e4d92ca8fd6eb35255a34dec73def7c731`,
  `fa3564a2aa6bca9deafbc64b21066988b922228bb58c5095552a5093bdcb1805`,
  `f2b50801d963a78ec4f31595ac2e4cdfeb3ab4c7226f58f8383ae6f0b7e2faca`
  and `e7f76126f5f57a8a289e4cb13fae12d26c34bcd96a24bdd47c234de5ae9fa19e`.
