# Scene collections and working views (Main Edit development)

The Visibility button opens a persistent, resizable panel. Clicking the viewport leaves it open and passes input to the editor. Drag its left edge to resize; use its close button or Escape to close it. Search filters the active tab; scroll inside the panel to inspect longer lists.

## Scene

Collections describe functional areas independently of motion assemblies. The mapped van document has eight roots: Vehicle shell, Roof equipment, Driver-side cabinetry, Passenger-side cabinetry, Rear workspace, Bed and seating, Electrical bay, and Water system. Expand a framed triangle to inspect subgroups and parts. The eye button hides a fully visible row and shows a hidden or mixed row; a crossed eye means hidden and an amber underline means mixed. Only isolates the complete selected area. Independent layer exclusions remain in effect. Restore removes temporary inspection exclusions and exits Only. Only retains independent layer exclusions.

Collection Show removes its own exclusion and preserves deliberately hidden child rows. Selecting a label displays the reason members remain hidden, including an area, covering layer, saved filter, or Only boundary. **Reveal selected** creates a scoped exception for authored-visible physical members of that collection or covering group, bypassing area, covering and saved-query exclusions without exposing unrelated areas. It retains Only and disabled Guides/References layers. Obsolete authored-hidden parts remain hidden. A later child Hide stays local to that child; hiding a whole area or covering layer cancels affected Reveal scopes; Restore clears all Reveal scopes. Scopes use durable membership, so future assigned imports participate. Working views retain nonempty Reveal scopes in an optional `revealed` list; older Main Edit builds do not understand that new inspection intent.

Vehicle shell includes the exterior body, roof skin and lining, and OEM interior proxies. Its visibility does not control solar equipment, roof fans, roof entry equipment, furniture, or placement transforms. Roof equipment is a separate collection. Authored hidden obsolete parts stay hidden unless explicitly shown.

## Layers

Layers control doors, drawers, access panels, nonphysical guides and reference geometry separately from functional area membership. Guide exclusions apply to walkway, clearance, movement and other nonphysical space. A material or an object's display name does not determine collection ownership.

## Views

Apply restores a named inspection state without changing geometry or adding Undo. The mapped van includes Interior (shell hidden), Driver-side cabinetry, Passenger-side kitchen, Sink cabinet, Electrical bay service, and Roof installation. Electrical service hides access panels and service doors. Roof installation retains roof placement surfaces and equipment.

Enable Edit to save the current state. New view clears the selected preset; enter a name and Save view. Selecting an existing preset changes the button to Update view. Delete view removes that preset. Preset changes are document edits with Undo and require File Save. Active legacy entity-query filters must be reset before capturing a view. Visibility toggles and applying a view remain temporary inspection state.

## Editing and STL imports

In Scene, enable Edit and select a collection label. Enter a name and use New child or Rename. Pick a model object in the viewport (or an assembly in Parts), then Add pick or Remove to edit explicit membership. Remove a collection's members and child collections before Delete. Membership can overlap: a bench housing can belong to seating and the electrical service area. Removing explicit membership does not remove inherited membership from a parent assembly.

The STL import options include a destination collection and a part role (Body, Door, Drawer, Access panel). Imported parts receive persistent membership; covering roles additionally participate in the corresponding layer. Assignment and placement share the import Undo operation. Assembly membership also includes future children added to that assembly. Unassigned physical objects remain discoverable instead of being guessed into a group.

Collections and views are stored in layout schema 28 with persistent object/assembly identifiers. Older documents load without collections; older app builds cannot read schema 28. Save the mapped van as a new document, preserving the original. Current bounds are 64 collections, 2048 membership entries, 16 working views and 128 explicit visibility masks per view.

This establishes stable areas for later dimensional inspection and build specifications. It does not yet produce fabrication drawings, cut lists or verified physical-fit dimensions.

**Inspect area** opens collection-based dimensional inspection and saved measurement controls. See [collection inspection](collection_inspection.md).
