from pathlib import Path
import math

import bpy

TRANSFORM_PATHS = {"location", "rotation_euler", "rotation_quaternion", "scale"}
LOCATION_EPSILON = 0.001
ROTATION_EPSILON_RADIANS = math.radians(10.0)
SCALE_EPSILON = 0.001


def write_vec3(file, v):
    file.write(f"{v.x:.9f} {v.y:.9f} {v.z:.9f}\n")


def write_quat(file, q):
    file.write(f"{q.w:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f}\n")


def vec_delta(a, b):
    return (a - b).length


def quat_angle_delta(a, b):
    dot = abs((a.w * b.w) + (a.x * b.x) + (a.y * b.y) + (a.z * b.z))
    dot = min(1.0, max(-1.0, dot))
    return 2.0 * math.acos(dot)


def get_rotation_quaternion(obj):
    if obj.rotation_mode == "QUATERNION":
        return obj.rotation_quaternion.copy()

    return obj.rotation_euler.to_quaternion()


def get_fcurves_by_path(action, path):
    return [
        fcurve for fcurve in iter_action_fcurves(action) if fcurve.data_path == path
    ]


def evaluate_rotation_quaternion(action, obj, frame):
    quaternion_fcurves = get_fcurves_by_path(action, "rotation_quaternion")
    if quaternion_fcurves:
        rotation = obj.rotation_quaternion.copy()

        for fcurve in quaternion_fcurves:
            if 0 <= fcurve.array_index < 4:
                rotation[fcurve.array_index] = fcurve.evaluate(frame)

        rotation.normalize()
        return rotation

    euler_fcurves = get_fcurves_by_path(action, "rotation_euler")
    if euler_fcurves:
        rotation = obj.rotation_euler.copy()

        for fcurve in euler_fcurves:
            if 0 <= fcurve.array_index < 3:
                rotation[fcurve.array_index] = fcurve.evaluate(frame)

        return rotation.to_quaternion()

    return get_rotation_quaternion(obj)


def get_transform_sample(action, obj, frame):
    return {
        "frame": frame,
        "location": obj.location.copy(),
        "rotation": evaluate_rotation_quaternion(action, obj, frame),
        "scale": obj.scale.copy(),
    }


def interpolate_transform_sample(start, end, frame):
    if end["frame"] == start["frame"]:
        factor = 0.0
    else:
        factor = (frame - start["frame"]) / (end["frame"] - start["frame"])

    return {
        "frame": frame,
        "location": start["location"].lerp(end["location"], factor),
        "rotation": start["rotation"].slerp(end["rotation"], factor),
        "scale": start["scale"].lerp(end["scale"], factor),
    }


def transform_error_ratio(sample, interpolated):
    location_ratio = vec_delta(sample["location"], interpolated["location"]) / LOCATION_EPSILON
    rotation_ratio = quat_angle_delta(sample["rotation"], interpolated["rotation"]) / ROTATION_EPSILON_RADIANS
    scale_ratio = vec_delta(sample["scale"], interpolated["scale"]) / SCALE_EPSILON
    return max(location_ratio, rotation_ratio, scale_ratio)


def add_required_sample_indices(samples, start_index, end_index, selected_indices):
    if end_index - start_index <= 1:
        return

    start = samples[start_index]
    end = samples[end_index]
    max_error_ratio = 0.0
    max_error_index = None

    for index in range(start_index + 1, end_index):
        sample = samples[index]
        interpolated = interpolate_transform_sample(start, end, sample["frame"])
        error_ratio = transform_error_ratio(sample, interpolated)

        if error_ratio > max_error_ratio:
            max_error_ratio = error_ratio
            max_error_index = index

    if max_error_index is None or max_error_ratio <= 1.0:
        return

    selected_indices.add(max_error_index)
    add_required_sample_indices(samples, start_index, max_error_index, selected_indices)
    add_required_sample_indices(samples, max_error_index, end_index, selected_indices)


def simplify_transform_samples(samples, authored_key_frames):
    selected_indices = {
        index
        for index, sample in enumerate(samples)
        if sample["frame"] in authored_key_frames
    }

    selected_indices.add(0)
    selected_indices.add(len(samples) - 1)

    anchors = sorted(selected_indices)

    for start_index, end_index in zip(anchors, anchors[1:]):
        add_required_sample_indices(samples, start_index, end_index, selected_indices)

    return [samples[index] for index in sorted(selected_indices)]


def debug_log(message):
    text = bpy.data.texts.get("transform_export_debug")
    if text is None:
        text = bpy.data.texts.new("transform_export_debug")
    text.write(str(message) + "\n")


def clear_debug_log():
    text = bpy.data.texts.get("transform_export_debug")
    if text is None:
        text = bpy.data.texts.new("transform_export_debug")
    text.clear()


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


def action_has_transform_animation(action):
    for fcurve in iter_action_fcurves(action):
        if fcurve.data_path in TRANSFORM_PATHS:
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

        if slot_id_type in (None, "UNSPECIFIED"):
            return True

        if owner_id_type is None or slot_id_type == owner_id_type:
            return True

    return False


def find_compatible_action_slot(action, owner):
    if not hasattr(action, "slots"):
        return None

    if len(action.slots) == 0:
        return None

    owner_id_type = getattr(owner, "id_type", None)
    compatible_slots = []

    for slot in action.slots:
        slot_id_type = getattr(slot, "target_id_type", None)

        if slot_id_type not in (None, "UNSPECIFIED"):
            if owner_id_type is not None and slot_id_type != owner_id_type:
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


def collect_transform_actions(owner):
    actions = []

    debug_log(f"Export owner: {owner.name} ({owner.type})")

    for action in bpy.data.actions:
        fcurve_paths = [fcurve.data_path for fcurve in iter_action_fcurves(action)]
        slot_info = [
            (
                getattr(slot, "target_id_type", None),
                getattr(slot, "name_display", ""),
                getattr(slot, "identifier", ""),
            )
            for slot in getattr(action, "slots", [])
        ]

        has_animation = action_has_animation(action)
        has_transform = action_has_transform_animation(action)
        has_slot = action_has_compatible_slot(action, owner)

        debug_log(
            {
                "action": action.name,
                "has_animation": has_animation,
                "has_transform": has_transform,
                "has_compatible_slot": has_slot,
                "fcurves": fcurve_paths,
                "slots": slot_info,
            }
        )

        if has_animation and has_transform and has_slot:
            actions.append(action)

    actions.sort(key=lambda action: action.name)
    return actions


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
        if fcurve.data_path not in TRANSFORM_PATHS:
            continue

        for keyframe in fcurve.keyframe_points:
            frames.add(int(round(keyframe.co.x)))

    if not frames:
        raise RuntimeError(
            f"Action '{action.name}' has no transform keyframes to export."
        )

    return sorted(frames)


def get_action_frame_range(key_pose_frames):
    return key_pose_frames[0], key_pose_frames[-1]


def safe_filename(name):
    keep = []

    for char in name:
        if char.isalnum() or char in ("_", "-"):
            keep.append(char)
        else:
            keep.append("_")

    return "".join(keep)


def export_spaceguy_3d(filepath, obj=None):
    clear_debug_log()

    if obj is None:
        obj = bpy.context.object

    if obj is None:
        raise RuntimeError("No active object selected")

    if obj.type != "MESH":
        raise RuntimeError(f"Active object must be a mesh, got {obj.type}")

    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()

    animation_owner = obj
    actions = collect_transform_actions(animation_owner)

    eval_obj = obj.evaluated_get(depsgraph)
    eval_mesh = eval_obj.to_mesh()

    source_vertex_count = len(eval_mesh.vertices)
    export_vertices, export_indices = collect_export_geometry(eval_obj, eval_mesh)

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
        file.write("spaceguy_3d_transform 2\n")
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
            file.write(f"{pos.x:.9f} {pos.y:.9f} {pos.z:.9f} {r:.6f} {g:.6f} {b:.6f}\n")

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
                authored_key_frames = set(key_pose_frames)
                start_frame, end_frame = get_action_frame_range(key_pose_frames)
                sample_frames = range(start_frame, end_frame + 1)

                assign_action(animation_owner, action)

                all_transform_samples = []

                for frame in sample_frames:
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

                    all_transform_samples.append(
                        get_transform_sample(action, animation_owner, frame)
                    )

                    eval_obj.to_mesh_clear()

                transform_samples = simplify_transform_samples(
                    all_transform_samples, authored_key_frames
                )

                file.write("\n")
                file.write(f"animation {safe_filename(action.name)}\n")
                loop = bool(action.get("loop", False))
                file.write(f"loop {'true' if loop else 'false'}\n")
                file.write(f"start_frame {start_frame}\n")
                file.write(f"end_frame {end_frame}\n")
                file.write(f"key_pose_count {len(transform_samples)}\n")

                for sample in transform_samples:
                    file.write(f"\nkey_pose {sample['frame']}\n")

                    file.write("location\n")
                    write_vec3(file, sample["location"])

                    file.write("rotation_quaternion\n")
                    write_quat(file, sample["rotation"])

                    file.write("scale\n")
                    write_vec3(file, sample["scale"])

        finally:
            if animation_owner.animation_data is not None:
                animation_owner.animation_data.action = original_action

                if hasattr(animation_owner.animation_data, "action_slot"):
                    animation_owner.animation_data.action_slot = original_action_slot

            scene.frame_set(original_frame)
            bpy.context.view_layer.update()

    debug_log(f"Exported {filepath}")
    debug_log(f"Vertices: {len(export_vertices)}")
    debug_log(f"Indices: {len(export_indices) * 3}")
    debug_log(f"Animations: {[action.name for action in actions]}")

    print(f"Exported {filepath}")
    print(f"Vertices: {len(export_vertices)}")
    print(f"Indices: {len(export_indices) * 3}")
    print("Animations:")

    if not actions:
        print("  none")

    for action in actions:
        key_pose_frames = get_action_key_pose_frames(action)
        start_frame, end_frame = get_action_frame_range(key_pose_frames)
        print(
            f"  {action.name}: {start_frame} -> {end_frame}, "
            f"{len(key_pose_frames)} key poses: {key_pose_frames}"
        )


blend_dir = Path(bpy.path.abspath("//"))
obj = bpy.context.object
output_path = blend_dir / f"{safe_filename(obj.name)}.3d"

export_spaceguy_3d(output_path)
