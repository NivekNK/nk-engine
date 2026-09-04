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
