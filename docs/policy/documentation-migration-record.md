# Documentation migration record

Status: Approved

Date: 2026-10-01
Classification: Approval and application record; does not define product behavior.

## Authorization and cleanup

The owner approved the 43 migration proposals, authorized their application,
and subsequently instructed: “i approve them, make thet all approved”.
The owner then requested cleanup of the retained temp.* files. This record
consolidates the approval/application evidence and exact target inventory.
All 45 temporary proposal/review files were removed after application.
No canonical product document was removed. Frozen references remain untouched.

## Created documents

The files below were created and marked Approved. The status is at line 3.

| File | Purpose |
| --- | --- |
| [README.md](../README.md) | Documentation entry point and navigation |
| [internal_spec/domain/behavior.md](../internal_spec/domain/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/domain/tech_notes.md](../internal_spec/domain/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/database/behavior.md](../internal_spec/database/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/database/tech_notes.md](../internal_spec/database/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/storage/behavior.md](../internal_spec/storage/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/storage/tech_notes.md](../internal_spec/storage/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/cases/behavior.md](../internal_spec/cases/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/cases/tech_notes.md](../internal_spec/cases/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/investigation/behavior.md](../internal_spec/investigation/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/investigation/tech_notes.md](../internal_spec/investigation/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/playbooks/behavior.md](../internal_spec/playbooks/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/playbooks/tech_notes.md](../internal_spec/playbooks/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/activity/behavior.md](../internal_spec/activity/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/activity/tech_notes.md](../internal_spec/activity/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/import-export/behavior.md](../internal_spec/import-export/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/import-export/tech_notes.md](../internal_spec/import-export/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/gui/behavior.md](../internal_spec/gui/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/gui/tech_notes.md](../internal_spec/gui/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/cli/behavior.md](../internal_spec/cli/behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [internal_spec/cli/tech_notes.md](../internal_spec/cli/tech_notes.md) | Compatibility, recovery, limitations, and maintenance constraints |
| [internal_spec/modules_registry.md](../internal_spec/modules_registry.md) | Module map and implementation status |
| [internal_spec/investigation/claims.md](../internal_spec/investigation/claims.md) | Claims, directional relations, and confirmation safeguards |
| [internal_spec/investigation/evidence.md](../internal_spec/investigation/evidence.md) | Sources, preservation, integrity, and evidence lifecycle |
| [templates/README.md](../templates/README.md) | Reusable English documentation template or template index |
| [templates/module-behavior.md](../templates/module-behavior.md) | Module purpose, responsibilities, behavior, and interfaces |
| [templates/module-tech-notes.md](../templates/module-tech-notes.md) | Reusable English documentation template or template index |
| [templates/documentation-proposal.md](../templates/documentation-proposal.md) | Reusable English documentation template or template index |
| [development/documentation-validation.md](../development/documentation-validation.md) | Documentation validation scope and CI follow-up |
| [reference/README.md](../reference/README.md) | Non-canonical reference index |

## Changed documents

Each changed document is Approved at line 3.

| File | Changes |
| --- | --- |
| [development/architecture.md](../development/architecture.md) | Approval metadata, implementation label, and module-registry link |
| [development/build.md](../development/build.md) | Approval metadata, build prerequisites, and verification qualification |
| [development/tests.md](../development/tests.md) | Approval metadata, documentation-check link, and dated verification limitations |
| [development/migrations.md](../development/migrations.md) | Approval metadata and implementation label |
| [user/cli.md](../user/cli.md) | Approval metadata and implementation label |
| [spec/acceptance.md](../spec/acceptance.md) | Approval metadata; historical acceptance distinguished from current verification |
| [design/product-vision.md](../design/product-vision.md) | Approval metadata; existing workspace distinguished from planned extensions |
| [design/gui/README.md](../design/gui/README.md) | Approval metadata and non-canonical visual-reference classification |
| [spec/evidence-trace-spec.md](../spec/evidence-trace-spec.md) | Version/date and approval metadata; Partial/Planned labels, implementation gaps, and corrected stale decisions |

## Moved references

The four files below moved without changing bytes. They remain non-canonical, and their contents were not adopted as project rules.

| Previous location | Current location | SHA-256 |
| --- | --- | --- |
| `docs/example1.md` | [reference/example1.md](../reference/example1.md) | `91ca2b69399274fa49a8a51f6dc8bb196a58b92839bbd14b47ff0c6c1cac6d9b` |
| `docs/example2.md` | [reference/example2.md](../reference/example2.md) | `3c2d55f1ef4a0c60d279565b5c80cbdfe4937b85d1a79ba5ea84db146492b647` |
| `docs/info_for_docs.pdf` | [reference/info_for_docs.pdf](../reference/info_for_docs.pdf) | `054fade3bf7e898dfec97b780f88a97205f03025ad9a2e583b7a09504eb031b6` |
| `docs/Claims_Evidens.png` | [reference/Claims_Evidens.png](../reference/Claims_Evidens.png) | `bed00952feaf2b8fa25b4bfb1e30b7c2fe59809cde340c152e445574cfe018f3` |

## Approval evidence

All 43 proposal target contents were applied exactly as approved. Current
maintained documents are Approved; reference/example contents are not adopted
by this migration. The documentation policy was already Approved and did not
change. The status-only update did not change product requirements or turn
planned functionality into implemented functionality.

The following SHA-256 hashes identify the exact applied Markdown targets.

| Target | SHA-256 |
| --- | --- |
| `docs/README.md` | `a39cbf952616b2d508a674871f75cc207eacffc382b255a15e96290d571e7fe1` |
| `docs/internal_spec/domain/behavior.md` | `cb53f46040691630afb47d8dd5b788c7984e70af7342e6f863da9e0f70d8a153` |
| `docs/internal_spec/domain/tech_notes.md` | `089a41e7f4322c12920cb866361c3b21fa4e2ea72bd1ec845b845caa5365bffb` |
| `docs/internal_spec/database/behavior.md` | `fb992580abb953b8e85e43bbbae1d8e66aaff4dcbb87fb74c9a1fa3c6542d3ee` |
| `docs/internal_spec/database/tech_notes.md` | `ada19588d363bdff2ab4e1f9c16e97b25bf5f30590d422c7e4685a259a4991a6` |
| `docs/internal_spec/storage/behavior.md` | `4a49f950b447a79b6dee0cb5d9086dd67ff9d8e8b486e2cff27590abb3481848` |
| `docs/internal_spec/storage/tech_notes.md` | `333ee82a30d8091a1bc29c3dc5a00fe9b413da94a931cec533995bc7d21b2f6a` |
| `docs/internal_spec/cases/behavior.md` | `13364b97cb2ac5fee85e8bbd1eae78e77a556f3ab3941b77b2d4d84ca8effb38` |
| `docs/internal_spec/cases/tech_notes.md` | `9eff0de9d44585518cf77e24c1117fda649515c7580bf4fd0cff5018bd6e1bae` |
| `docs/internal_spec/investigation/behavior.md` | `f11aab04c42772e2f531dfa1de3afe48bb9105fc7f49e67cb0689337b27ade24` |
| `docs/internal_spec/investigation/tech_notes.md` | `e5fda98aa9c75b2e39e46996d5111c07f3a282ea2e9215dfb763ecd16bace2ed` |
| `docs/internal_spec/playbooks/behavior.md` | `be18850fa810973dc16c6804ab2c1f21dacc41ddb08c69aa83f4d067283a78fa` |
| `docs/internal_spec/playbooks/tech_notes.md` | `b186adab2736a3aef3c543d30b917f5da45ae63e8523186994b7da96cd90e8c0` |
| `docs/internal_spec/activity/behavior.md` | `049a2c8c51f83e2ab9837738f0465cb6104a016dfec3784705aa43ec2ed70fbc` |
| `docs/internal_spec/activity/tech_notes.md` | `f25184e6e7d5909c376c598455f050fce977f78e86c2c0c6df54701bc8524a45` |
| `docs/internal_spec/import-export/behavior.md` | `cdda1a7d20dd6d3db5ccc813368f4a29b283e0d0238322d8cc318dd3a6a59223` |
| `docs/internal_spec/import-export/tech_notes.md` | `82369f626ae477dab8732cde3e3ff149277286371437d435558485c92335438c` |
| `docs/internal_spec/gui/behavior.md` | `ed40b419963e9c99b8d679dcfd7109745bddf2b3bddb4d235833b8597735c5c2` |
| `docs/internal_spec/gui/tech_notes.md` | `b63620665f434a797690b83ede55c023dbc88b5b97c3e881606674647da0cb50` |
| `docs/internal_spec/cli/behavior.md` | `d37dffdd57299bcebcc29b2a442a10ea0a79bfd3f8546c85b7547c938a583247` |
| `docs/internal_spec/cli/tech_notes.md` | `d383807dd76a8aa5778238293ad933494f4ca2a5f72b701dab8252c806548d0e` |
| `docs/internal_spec/modules_registry.md` | `3770adfcd5f5c96947b66a5ad7d7ad10080304bd34289ab7909eb8a308d00ef8` |
| `docs/internal_spec/investigation/claims.md` | `96681b0a7b68cc6f5d75f50abf53d7cbbefb4629e908f19c971724c0c5bf1a1f` |
| `docs/internal_spec/investigation/evidence.md` | `66c2431db6af7900f9ca9da31c98290127d559996bc3b1a16ea2c6459129008e` |
| `docs/development/architecture.md` | `751b29c66aff07ca9d847e8d1ddb2120ee27352a1a9b75b8c46e514c323e4964` |
| `docs/development/build.md` | `aa985e790dc4185263a6a2a5b08428a8afa6f9efd0fa6e5198af381a42636c21` |
| `docs/development/tests.md` | `f0ff65c0e5db5749498059e462a99ce11a0becc4eb81ebf65eb937376a8053da` |
| `docs/development/migrations.md` | `85a39eaedd8e8cb9b1281cc8cb2149b11229bb69b6a5ea0e5d641df85d8bff4c` |
| `docs/user/cli.md` | `212310843586584208514ade93120de389b8083f0efde913d87f32ca2cd4d7f8` |
| `docs/spec/acceptance.md` | `02291cd08647c248fa76cff705e1bc3a5fdef792cdfa76f557ec25586c8c5c24` |
| `docs/design/product-vision.md` | `c3b9a172e2920d410df8d7e19a4fa653c8525d30a0c2fc23942b82da2c85a46b` |
| `docs/design/gui/README.md` | `b8f996f080a4d7888c3552ff519969625d385e9278b7160115c55a37ad97e588` |
| `docs/spec/evidence-trace-spec.md` | `1f7eee708f532c7ef44e6b48f4790ee291763c99b8564f9868d67ce6faacde68` |
| `docs/templates/README.md` | `b6b252e3c14c9c12a32f69ae792724cc914e7b8cb6c6b28f98073054e532d7ee` |
| `docs/templates/module-behavior.md` | `789646d2b2f23cd00e8147103ed7f488231483e0e39e9ebfe68aac5cd9231d57` |
| `docs/templates/module-tech-notes.md` | `f709f3d2c502257fc478ee7d912a380c2e6cb838d8d2aa548ae4cfe90fa71b95` |
| `docs/templates/documentation-proposal.md` | `b84066ee7b26890769da9a7eb11da220e65db4286bfe82535f97cbae7802dbff` |
| `docs/development/documentation-validation.md` | `77a304a03504789f078a2c2c17a2cc9fffe34bf45686de9927d29cffaf7b27d2` |
| `docs/reference/README.md` | `8931b2727e6f00d34377861eccc6011a03f52640095a28c5c6a6e0032f32a4fb` |

## Verification and remaining work

Applied files matched the approved target content. Local links and heading
fragments, document statuses, UTF-8, titles, balanced fences, trailing whitespace,
and template-aware placeholder checks passed. Four moves preserved bytes;
nine unaffected originals, including the frozen Ukrainian specification and
approved policy, remained unchanged.

The existing integration executable passed without a rebuild during preparation.
No fresh build, full Markdown lint, native GUI interaction, or performance
benchmark is claimed. Repository-root agent instructions and automated
documentation CI remain separate work. No product code changed.
