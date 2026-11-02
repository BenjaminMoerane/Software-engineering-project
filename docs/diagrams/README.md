# Report diagrams (draw.io)

Every figure in the final report has an editable source here. Open a `.drawio` file at https://app.diagrams.net (File → Open from → Device) or in the draw.io desktop app, edit it, then use File → Export as → PNG to refresh the matching `.png`.

| Report figure | Source | Notes |
|---|---|---|
| 2.1 Gantt chart | `fig2_1_gantt.drawio` | From the Phase 1 plan; critical path A–H, gates |
| 3.1 Use case diagram | `fig3_1_use_case.drawio` | Four actors; abstract *User* generalisation; «include» / «extend» |
| 4.1 Component diagram | `fig4_1_component.drawio` | Layered modular monolith |
| 4.2a Class diagram — services and patterns | `fig4_2a_class_design.drawio` | Facade, Strategy, Repository; signatures match `src/` at v1.1.1 |
| 4.2b Class diagram — domain model | `fig4_2b_class_domain.drawio` | Entities, multiplicities; 1:1 with tables |
| 4.3 State machine — Submission | `fig4_3_state_submission.drawio` | |
| 4.4 Activity diagram with swimlanes | `fig4_4_activity.drawio` | Student, System, Lecturer, HoD |
| 4.5 Sequence — Submit assignment | `fig4_5_seq_submit.drawio` | Late and resubmission paths (alt) |
| 4.6 Sequence — Moderate and release | `fig4_6_seq_moderate.drawio` | |
| 4.7 ERD (3NF) | `fig4_7_erd.drawio` | Columns generated from `database_schema.sql` |
| 8.1 Deployment diagram | `fig8_1_deployment.drawio` | Supersedes `docs/deployment/deployment_diagram.*` |

Items new in v1.1 (CR1) are drawn with a thick blue border.
