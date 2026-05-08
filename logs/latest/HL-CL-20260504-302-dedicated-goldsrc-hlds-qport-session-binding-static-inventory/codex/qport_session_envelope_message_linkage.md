# Qport/Session Envelope-Message Linkage

| Area | Local evidence | Qport/session appears? | Interpretation | Needed before fixture contract? |
| --- | --- | --- | --- | --- |
| Message writing helpers | prompt 299 callback/query-info/pseudo helpers | no real qport writer | Helpers do not encode qport/session. | yes |
| Reliable/unreliable envelope candidates | prompt 301 matrix | qport/session marked unknown dependency | Envelope cannot be placed without session binding evidence. | yes |
| Sequence/ack candidates | prompt 300 matrix | qport/client port row only, no qport bytes | Sequence/ack cannot be safely fixed to a peer without session binding. | yes |
| Address-scoped challenge cache | prompt 269/source | endpoint key exists | Reusable as diagnostic prerequisite, not real proof. | yes, as prerequisite guard |
| Userinfo validation policy | prompt 270/source | no qport, but connect text identity exists | Userinfo is a diagnostic connect prerequisite only. | yes, as policy guard |
| Query/info boundary | prompts 287-296 | no qport/session dependency | Connectionless query/info stays separate. | no for query/info, yes for post-connect/signon |

envelope_message_linkage_rows_count: 6
