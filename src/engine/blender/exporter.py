import bpy
from pathlib import Path


def write_vec3(file, v):
    file.write(f"{v.x:.9f} {v.y:.9f} {v.z:.9f}\n")


def get_material_color(material):
    if material is None:
        return (1.0, 1.0, 1.0)

    if material.use_nodes and material.node_tree is not None:
        for node in material.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                base_color = node.inputs.get("Base Color")

                if base_color is not None:
                    color = base_color.default_value
                    return (float(color[0]), float(color[1]), float(color[2]))

    color = material.diffuse_color
    return (float(color[0]), float(color[1]), float(color[2]))


def get_polygon_material_color(obj, polygon):
    if polygon.material_index < len(obj.material_slots):
        material = obj.material_slots[polygon.material_index].material
        return get_material_color(material)

    return (1.0, 1.0, 1.0)


def collect_export_geometry(eval_obj, eval_mesh):
    eval_mesh.calc_loop_triangles()

    export_vertices = []
    export_indices = []
    vertex_map = {}

    for tri in eval_mesh.loop_triangles:
        polygon = eval_mesh.polygons[tri.polygon_index]
        color = get_polygon_material_color(eval_obj, polygon)

        tri_indices = []

        for source_vertex_index in tri.vertices:
            key = (source_vertex_index, color)

            if key not in vertex_map:
                export_index = len(export_vertices)
                vertex_map[key] = export_index

                pos = eval_mesh.vertices[source_vertex_index].co.copy()
                export_vertices.append((source_vertex_index, pos, color))

            tri_indices.append(vertex_map[key])

        export_indices.append(tuple(tri_indices))

    return export_vertices, export_indices


def get_animation_owner(obj):
    if obj.parent is not None and obj.parent.type == "ARMATURE":
        return obj.parent

    for modifier in obj.modifiers:
        if modifier.type == "ARMATURE" and modifier.object is not None:
            return modifier.object

    return obj


def ensure_animation_data(owner):
    if owner.animation_data is None:
        owner.animation_data_create()


def iter_action_fcurves(action):
    if hasattr(action, "fcurves"):
        for fcurve in action.fcurves:
            yield fcurve

    if not hasattr(action, "layers"):
        return

    for layer in action.layers:
        for strip in getattr(layer, "strips", []):
            for channelbag in getattr(strip, "channelbags", []):
                for fcurve in getattr(channelbag, "fcurves", []):
                    yield fcurve


def action_has_animation(action):
    if hasattr(action, "is_empty"):
        return not action.is_empty

    for _fcurve in iter_action_fcurves(action):
        return True

    return False


def action_has_compatible_slot(action, owner):
    if not hasattr(action, "slots"):
        return True

    if len(action.slots) == 0:
        return True

    owner_id_type = getattr(owner, "id_type", None)

    for slot in action.slots:
        slot_id_type = getattr(slot, "target_id_type", None)
        if slot_id_type is None or owner_id_type is None or slot_id_type == owner_id_type:
            return True

    return False


def collect_actions(owner):
    if owner.animation_data is None:
        return []

    actions = [
        action
        for action in bpy.data.actions
        if action_has_animation(action) and action_has_compatible_slot(action, owner)
    ]

    actions.sort(key=lambda action: action.name)
    return actions


def find_compatible_action_slot(action, owner):
    if not hasattr(action, "slots"):
        return None

    if len(action.slots) == 0:
        return None

    owner_id_type = getattr(owner, "id_type", None)
    compatible_slots = []

    for slot in action.slots:
        slot_id_type = getattr(slot, "target_id_type", None)

        if slot_id_type is not None and owner_id_type is not None:
            if slot_id_type not in (owner_id_type, "UNSPECIFIED"):
                continue

        compatible_slots.append(slot)

    if not compatible_slots:
        return None

    for slot in compatible_slots:
        slot_name = getattr(slot, "name_display", "")
        slot_identifier = getattr(slot, "identifier", "")

        if slot_name == owner.name or slot_identifier.endswith(owner.name):
            return slot

    return compatible_slots[0]


def assign_action(owner, action):
    ensure_animation_data(owner)

    animation_data = owner.animation_data
    animation_data.action = action

    slot = find_compatible_action_slot(action, owner)
    if slot is not None and hasattr(animation_data, "action_slot"):
        animation_data.action_slot = slot


def get_action_key_pose_frames(action):
    frames = set()

    for fcurve in iter_action_fcurves(action):
        for keyframe in fcurve.keyframe_points:
            frames.add(int(round(keyframe.co.x)))

    if not frames:
        raise RuntimeError(
            f"Action '{action.name}' has no keyframes to export as key poses."
        )

    return sorted(frames)


def get_key_pose_frame_range(key_pose_frames):
    return key_pose_frames[0], key_pose_frames[-1]


def export_spaceguy_3d(filepath, obj=None):
    if obj is None:
        obj = bpy.context.object

    if obj is None:
        raise RuntimeError("No active object selected")

    if obj.type != "MESH":
        raise RuntimeError(f"Active object must be a mesh, got {obj.type}")

    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()

    animation_owner = get_animation_owner(obj)
    actions = collect_actions(animation_owner)

    eval_obj = obj.evaluated_get(depsgraph)
    eval_mesh = eval_obj.to_mesh()

    source_vertex_count = len(eval_mesh.vertices)
    export_vertices, export_indices = collect_export_geometry(
        eval_obj, eval_mesh)

    eval_obj.to_mesh_clear()

    fps = scene.render.fps / scene.render.fps_base
    filepath = Path(filepath)

    original_frame = scene.frame_current
    original_action = None
    original_action_slot = None

    if animation_owner.animation_data is not None:
        original_action = animation_owner.animation_data.action

        if hasattr(animation_owner.animation_data, "action_slot"):
            original_action_slot = animation_owner.animation_data.action_slot

    with filepath.open("w", encoding="utf-8") as file:
        file.write("spaceguy_3d 2\n")
        file.write(f"object_name {safe_filename(obj.name)}\n")
        file.write(f"fps {fps:.6f}\n")
        file.write(f"vertex_count {len(export_vertices)}\n")
        file.write(f"index_count {len(export_indices) * 3}\n")
        file.write(f"animation_count {len(actions)}\n")
        file.write("\n")

        file.write("vertices\n")
        file.write("# x y z r g b\n")
        for _source_vertex_index, pos, color in export_vertices:
            r, g, b = color
            file.write(
                f"{pos.x:.9f} {pos.y:.9f} {pos.z:.9f} "
                f"{r:.6f} {g:.6f} {b:.6f}\n"
            )

        file.write("\n")
        file.write("indices\n")
        file.write("# i0 i1 i2\n")
        for i0, i1, i2 in export_indices:
            file.write(f"{i0} {i1} {i2}\n")

        file.write("\n")
        file.write("animations\n")

        try:
            for action in actions:
                key_pose_frames = get_action_key_pose_frames(action)
                start_frame, end_frame = get_key_pose_frame_range(
                    key_pose_frames)

                assign_action(animation_owner, action)

                file.write("\n")
                file.write(f"animation {safe_filename(action.name)}\n")
                file.write(f"start_frame {start_frame}\n")
                file.write(f"end_frame {end_frame}\n")
                file.write(f"key_pose_count {len(key_pose_frames)}\n")

                for frame in key_pose_frames:
                    scene.frame_set(frame)
                    bpy.context.view_layer.update()

                    depsgraph = bpy.context.evaluated_depsgraph_get()
                    eval_obj = obj.evaluated_get(depsgraph)
                    eval_mesh = eval_obj.to_mesh()

                    if len(eval_mesh.vertices) != source_vertex_count:
                        eval_obj.to_mesh_clear()
                        raise RuntimeError(
                            f"Animation {action.name}, key pose frame {frame} has "
                            f"{len(eval_mesh.vertices)} vertices, expected "
                            f"{source_vertex_count}. Animated topology is not supported."
                        )

                    file.write(f"\nkey_pose {frame}\n")
                    for source_vertex_index, _base_pos, _color in export_vertices:
                        vertex = eval_mesh.vertices[source_vertex_index]
                        write_vec3(file, vertex.co)

                    eval_obj.to_mesh_clear()

        finally:
            if animation_owner.animation_data is not None:
                animation_owner.animation_data.action = original_action

                if hasattr(animation_owner.animation_data, "action_slot"):
                    animation_owner.animation_data.action_slot = original_action_slot

            scene.frame_set(original_frame)
            bpy.context.view_layer.update()

    print(f"Exported {filepath}")
    print(f"Vertices: {len(export_vertices)}")
    print(f"Indices: {len(export_indices) * 3}")
    print("Animations:")

    if not actions:
        print("  none")

    for action in actions:
        key_pose_frames = get_action_key_pose_frames(action)
        start_frame, end_frame = get_key_pose_frame_range(key_pose_frames)
        print(
            f"  {action.name}: {start_frame} -> {end_frame}, "
            f"{len(key_pose_frames)} key poses: {key_pose_frames}"
        )


def safe_filename(name):
    keep = []
    for char in name:
        if char.isalnum() or char in ("_", "-"):
            keep.append(char)
        else:
            keep.append("_")
    return "".join(keep)


blend_dir = Path(bpy.path.abspath("//"))
obj = bpy.context.object
output_path = blend_dir / f"{safe_filename(obj.name)}.3d"

export_spaceguy_3d(output_path)
