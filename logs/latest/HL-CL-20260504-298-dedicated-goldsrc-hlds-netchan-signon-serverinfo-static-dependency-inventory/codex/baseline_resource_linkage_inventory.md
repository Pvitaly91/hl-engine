# Baseline And Resource Linkage Inventory

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

| Linkage | Static references found | Classification | Required before signon-time serverinfo? | Byte-level known? | Candidate next inventory? |
| --- | --- | --- | --- | --- | --- |
| Model precache | PrecacheRegistry::PrecacheModel, ModelIndex, EnsureModelIndex, ModelName, ModelCount | Host scaffolding | Likely yes for resource/model baselines | No | Yes |
| Sound precache | PrecacheRegistry::PrecacheSound, SoundName, SoundCount, SoundPrecacheRegistry references | Host scaffolding | Likely yes | No | Yes |
| Event baseline | Broad event symbols exist, but no clear signon event-baseline serializer found | Unknown/diagnostic | Likely yes | No | Yes |
| Resource list | resource_baselines_not_sent gates and resource path normalization | Diagnostic blocker plus host utility | Likely yes | No | Yes |
| Entity/edict baseline | EdictStore::*, entity snapshots, edict indices/offsets | Host scaffolding | Likely yes | No | Yes |
| Map CRC/checksum | Not established by this scan as serverinfo byte evidence | Not found/unknown | Unknown but likely needed | No | Yes |
| Server spawn count/server count | Summary fields such as spawn_count exist | Diagnostic/report state | Likely relevant | No | Yes |
| Client slot/index | Some client-slot references; no real admission serializer | Diagnostic/report state | Likely relevant | No | Yes |

Baseline/resource symbols found: 886 aggregate baseline plus resource matches in the selected scan. This is not byte-level baseline evidence.
