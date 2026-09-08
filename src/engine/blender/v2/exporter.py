import math
import os
import shlex
import tempfile
from pathlib import Path

import bpy

TRANSFORM_PATHS = {"location", "rotation_euler", "rotation_quaternion", "scale"}
LOCATION_EPSILON = 0.001
ROTATION_EPSILON_RADIANS = math.radians(10.0)
SCALE_EPSILON = 0.001


def write_vec3(file, v):
    file.write(f"{v.x:.9f} {v.y:.9f} {v.z:.9f}\n")


def write_quat(file, q):
    file.write(f"{q.w:.9f} {q.x:.9f} {q.y:.9f} {q.z:.9f}\n")


def single_link(socket, description):
    if socket is None or len(socket.links) != 1:
        raise RuntimeError(f"{description} must have exactly one connection.")

    return socket.links[0]


def get_surface(obj, mesh):
    if len(obj.material_slots) != 1:
        raise RuntimeError(f"Object '{obj.name}' must have exactly one material slot.")

    if any(polygon.material_index != 0 for polygon in mesh.polygons):
        raise RuntimeError(f"Object '{obj.name}' uses another material slot.")

    material = obj.material_slots[0].material

    if material is None or not material.use_nodes or material.node_tree is None:
        raise RuntimeError(f"Object '{obj.name}' needs a node material.")

    outputs = [
        node
        for node in material.node_tree.nodes
        if node.type == "OUTPUT_MATERIAL" and node.is_active_output
    ]

    if len(outputs) != 1:
        raise RuntimeError("Material must have one active Material Output.")

    shader_link = single_link(
        outputs[0].inputs.get("Surface"),
        "Material Output Surface",
    )
    shader = shader_link.from_node

    if shader.type != "BSDF_DIFFUSE":
        raise RuntimeError(
            "Material Output Surface must connect directly to Diffuse BSDF."
        )

    color_link = single_link(
        shader.inputs.get("Color"),
        "Diffuse BSDF Color",
    )
    texture = color_link.from_node

    if texture.type != "TEX_IMAGE" or color_link.from_socket.name != "Color":
        raise RuntimeError(
            "Diffuse BSDF Color must connect directly to Image Texture Color."
        )

    if texture.projection != "FLAT":
        raise RuntimeError("Only flat UV image projection is supported.")

    image = texture.image

    if image is None or image.source != "FILE":
        raise RuntimeError("The texture must reference a saved, single image.")

    if image.is_dirty:
        raise RuntimeError(f"Save changes to image '{image.name}' before export.")

    image_path = Path(bpy.path.abspath(image.filepath, library=image.library)).resolve()

    if not image_path.is_file():
        raise RuntimeError(f"Texture image does not exist on disk: {image_path}")

    vector_input = texture.inputs.get("Vector")

    if vector_input is None:
        raise RuntimeError("Image Texture has no Vector input.")

    if vector_input.is_linked:
        uv_link = single_link(vector_input, "Image Texture Vector")
        uv_node = uv_link.from_node

        if (
            uv_node.type != "UVMAP"
            or uv_link.from_socket.name != "UV"
            or uv_node.from_instancer
        ):
            raise RuntimeError(
                "Texture coordinates must come directly from a UV Map node "
                "or use the default render UV map."
            )

        uv_name = uv_node.uv_map
    else:
        uv_name = ""

    if uv_name:
        uv_layer = mesh.uv_layers.get(uv_name)
    else:
        uv_layer = next(
            (layer for layer in mesh.uv_layers if layer.active_render),
            None,
        )

    if uv_layer is None:
        raise RuntimeError(f"Object '{obj.name}' has no usable UV map.")

    return image_path, uv_layer.name


def get_corner_uv(mesh, uv_name, loop_index):
    layer = mesh.uv_layers.get(uv_name)

    if layer is None:
        raise RuntimeError(f"Missing UV map '{uv_name}'.")

    # Current Blender API, with compatibility for older Blender versions.
    if hasattr(layer, "uv"):
        return layer.uv[loop_index].vector.copy()

    return layer.data[loop_index].uv.copy()


def get_corner_normal(mesh, loop_index):
    try:
        return mesh.corner_normals[loop_index].vector.copy()
    except AttributeError:
        return mesh.loops[loop_index].normal.copy()


def quote_path(path):
    value = str(path).replace("\\", "/")

    if "\n" in value or "\r" in value:
        raise RuntimeError("Texture paths cannot contain line breaks.")

    return '"' + value.replace('"', '\\"') + '"'


def collect_export_geometry(mesh, uv_name):
    mesh.calc_loop_triangles()

    export_vertices = []
    export_indices = []
    vertex_map = {}

    triangles = sorted(
        mesh.loop_triangles,
        key=lambda tri: (
            tuple(sorted(tri.vertices)),
            tri.polygon_index,
        ),
    )

    for triangle in triangles:
        indices = []

        for loop_index in triangle.loops:
            if loop_index not in vertex_map:
                source_vertex_index = mesh.loops[loop_index].vertex_index

                position = mesh.vertices[source_vertex_index].co.copy()
                uv = get_corner_uv(mesh, uv_name, loop_index)
                normal = get_corner_normal(mesh, loop_index)

                vertex_map[loop_index] = len(export_vertices)

                export_vertices.append(
                    (
                        source_vertex_index,
                        loop_index,
                        position,
                        uv,
                        normal,
                    )
                )

            indices.append(vertex_map[loop_index])

        export_indices.append(tuple(indices))

    return export_vertices, export_indices


def topology_signature(mesh):
    return (
        len(mesh.vertices),
        tuple(loop.vertex_index for loop in mesh.loops),
        tuple((polygon.loop_start, polygon.loop_total) for polygon in mesh.polygons),
    )


def capture_surface_layout(obj, mesh):
    image_path, uv_name = get_surface(obj, mesh)

    return {
        "image_path": image_path,
        "uv_name": uv_name,
        "topology": topology_signature(mesh),
        "uvs": tuple(
            tuple(get_corner_uv(mesh, uv_name, index))
            for index in range(len(mesh.loops))
        ),
    }


def validate_surface_layout(obj, mesh, reference):
    current = capture_surface_layout(obj, mesh)

    if current["image_path"] != reference["image_path"]:
        raise RuntimeError("Animated texture references are not supported.")

    if current["uv_name"] != reference["uv_name"]:
        raise RuntimeError("Animated UV map selection is not supported.")

    labels = (
        "vertex count",
        "loop-to-vertex connections",
        "polygon loop ranges",
    )

    for label, actual, expected in zip(
        labels,
        current["topology"],
        reference["topology"],
    ):
        if actual != expected:
            raise RuntimeError(
                f"Frame {bpy.context.scene.frame_current}: {label} changed."
            )

    for actual, expected in zip(current["uvs"], reference["uvs"]):
        if any(abs(a - b) > 1e-7 for a, b in zip(actual, expected)):
            raise RuntimeError("Animated UV coordinates are not supported.")


def validate_evaluated_surface(obj, reference):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()

    try:
        validate_surface_layout(evaluated, mesh, reference)
    finally:
        evaluated.to_mesh_clear()


def is_bone_attachment(obj, armature):
    if armature is None or armature.type != "ARMATURE":
        return False

    for pose_bone in armature.pose.bones:
        if get_bone_attachment(pose_bone) == obj:
            return True

    return False


def get_animation_owner(obj):
    if (
        obj.parent is not None
        and obj.parent.type == "ARMATURE"
        and is_bone_attachment(obj, obj.parent)
    ):
        return obj

    for modifier in obj.modifiers:
        if modifier.type == "ARMATURE" and modifier.object is not None:
            if not is_bone_attachment(obj, modifier.object):
                return modifier.object

    if obj.parent is not None and obj.parent.type == "ARMATURE":
        return obj.parent

    return obj


def get_bone_attachment(pose_bone):
    pose_child = pose_bone.get("child_of_bone")
    bone_child = pose_bone.bone.get("child_of_bone")

    if pose_child is not None and bone_child is not None and pose_child != bone_child:
        raise RuntimeError(
            f"Bone '{pose_bone.name}' has conflicting child_of_bone properties."
        )

    child = pose_child if pose_child is not None else bone_child
    if child is None:
        return None

    if not isinstance(child, bpy.types.Object):
        raise RuntimeError(
            f"Bone '{pose_bone.name}' child_of_bone must reference an Object."
        )

    return child


def collect_bone_attachments(animation_owner):
    if animation_owner.type != "ARMATURE":
        return []

    attachments = []
    child_to_bone = {}

    for pose_bone in animation_owner.pose.bones:
        child = get_bone_attachment(pose_bone)
        if child is None:
            continue

        child_pointer = child.as_pointer()
        if child_pointer in child_to_bone:
            raise RuntimeError(
                f"Object '{child.name}' is referenced by child_of_bone on both "
                f"'{child_to_bone[child_pointer]}' and '{pose_bone.name}'."
            )

        child_to_bone[child_pointer] = pose_bone.name
        attachments.append({"bone_name": pose_bone.name, "child": child})

    return attachments


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


def action_has_vertex_animation(action):
    for fcurve in iter_action_fcurves(action):
        if fcurve.data_path not in TRANSFORM_PATHS:
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

    for slot in action.slots:
        if owner in slot.users():
            return slot

    owner_id_type = getattr(owner, "id_type", None)

    compatible_slots = [
        slot
        for slot in action.slots
        if getattr(slot, "target_id_type", None) in (None, "UNSPECIFIED", owner_id_type)
    ]

    for slot in compatible_slots:
        if getattr(slot, "name_display", "") == owner.name:
            return slot

        if getattr(slot, "identifier", "").endswith(owner.name):
            return slot

    return None


def assign_action(owner, action):
    ensure_animation_data(owner)

    animation_data = owner.animation_data
    animation_data.action = action

    slot = find_compatible_action_slot(action, owner)
    if slot is not None and hasattr(animation_data, "action_slot"):
        animation_data.action_slot = slot


def action_is_owned_by(action, owner):
    if not hasattr(action, "slots"):
        return False

    for slot in action.slots:
        if owner in slot.users():
            return True

    return False


def collect_owned_actions(owner, kind):
    candidate_actions = []

    # Find every Action that Blender associates with this owner.
    for action in bpy.data.actions:
        if action_is_owned_by(action, owner):
            candidate_actions.append(action)

    # Compatibility fallback for older/legacy Actions.
    if owner.animation_data is not None:
        if owner.animation_data.action is not None:
            candidate_actions.append(owner.animation_data.action)

        for track in owner.animation_data.nla_tracks:
            for strip in track.strips:
                if strip.action is not None:
                    candidate_actions.append(strip.action)

    actions = []
    seen = set()

    for action in candidate_actions:
        action_id = action.as_pointer()

        if action_id in seen:
            continue

        seen.add(action_id)

        if not action_has_animation(action):
            continue

        if not action_has_compatible_slot(action, owner):
            continue

        if kind == "vertex" and not action_has_vertex_animation(action):
            continue

        if kind == "transform" and not action_has_transform_animation(action):
            continue

        actions.append(action)

    return actions


def collect_export_clips(obj, filepath):
    clips = []

    animation_owner = get_animation_owner(obj)
    if animation_owner != obj:
        for action in collect_owned_actions(animation_owner, "vertex"):
            clips.append(
                {
                    "name": safe_filename(action.name),
                    "action": action,
                    "kind": "vertex",
                    "owner": animation_owner,
                    "attachments": collect_bone_attachments(animation_owner),
                }
            )

    for action in collect_owned_actions(obj, "transform"):
        clips.append(
            {
                "name": safe_filename(action.name),
                "action": action,
                "kind": "transform",
                "owner": obj,
            }
        )

    return order_clips_by_existing_manifest(clips, filepath)


def read_tokens_ignoring_comments(filepath):
    tokens = []

    try:
        with Path(filepath).open("r", encoding="utf-8") as file:
            for line in file:
                stripped = line.strip()
                if not stripped or stripped.startswith("#"):
                    continue
                tokens.extend(shlex.split(stripped, comments=False, posix=True))
    except FileNotFoundError:
        return []

    return tokens


class TokenReader:
    def __init__(self, tokens):
        self.tokens = tokens
        self.index = 0

    def next(self):
        if self.index >= len(self.tokens):
            raise RuntimeError("Unexpected end of file")
        token = self.tokens[self.index]
        self.index += 1
        return token

    def expect(self, expected):
        actual = self.next()
        if actual != expected:
            raise RuntimeError(f"Expected '{expected}', got '{actual}'")

    def read_int(self):
        return int(self.next())


def skip_vertex_clip(reader, vertex_count, version):
    reader.expect("loop")
    reader.next()
    reader.expect("start_frame")
    reader.next()
    reader.expect("end_frame")
    reader.next()
    reader.expect("key_pose_count")
    key_pose_count = reader.read_int()
    values_per_vertex = 6 if version >= 6 else 3

    for _pose in range(key_pose_count):
        reader.expect("key_pose")
        reader.next()
        for _ in range(vertex_count * values_per_vertex):
            reader.next()


def skip_attachment_sample(reader):
    reader.expect("key_pose")
    reader.next()
    reader.expect("location")
    for _value in range(3):
        reader.next()
    reader.expect("rotation_quaternion")
    for _value in range(4):
        reader.next()
    reader.expect("scale")
    for _value in range(3):
        reader.next()


def skip_attachments(reader):
    reader.expect("attachment_count")
    attachment_count = reader.read_int()

    for _attachment in range(attachment_count):
        reader.expect("attachment")
        reader.next()
        reader.expect("bone")
        reader.next()
        reader.expect("key_pose_count")
        key_pose_count = reader.read_int()

        for _pose in range(key_pose_count):
            skip_attachment_sample(reader)


def skip_transform_clip(reader):
    reader.expect("loop")
    reader.next()
    reader.expect("start_frame")
    reader.next()
    reader.expect("end_frame")
    reader.next()
    reader.expect("key_pose_count")
    key_pose_count = reader.read_int()

    for _pose in range(key_pose_count):
        reader.expect("key_pose")
        reader.next()
        reader.expect("location")
        for _value in range(3):
            reader.next()
        rotation_token = reader.next()
        if rotation_token == "rotation_quaternion":
            for _value in range(4):
                reader.next()
        elif rotation_token == "rotation":
            for _value in range(3):
                reader.next()
        else:
            raise RuntimeError(f"Expected rotation token, got '{rotation_token}'")
        reader.expect("scale")
        for _value in range(3):
            reader.next()


def read_existing_animation_manifest(filepath):
    tokens = read_tokens_ignoring_comments(filepath)
    if not tokens:
        return []

    reader = TokenReader(tokens)
    magic = reader.next()
    version = reader.read_int()

    if magic == "spaceguy_3d" and version == 2:
        default_kind = "vertex"
        unified = False
    elif magic == "spaceguy_3d_transform" and version in (1, 2):
        default_kind = "transform"
        unified = False
    elif magic == "spaceguy_3d" and version in (3, 4, 5, 6, 7):
        default_kind = None
        unified = True
    else:
        return []

    reader.expect("object_name")
    reader.next()

    if magic == "spaceguy_3d" and version == 7:
        reader.expect("texture_path")
        reader.next()

    reader.expect("fps")
    reader.next()
    reader.expect("vertex_count")
    vertex_count = reader.read_int()
    reader.expect("index_count")
    index_count = reader.read_int()
    reader.expect("animation_count")
    animation_count = reader.read_int()

    reader.expect("vertices")
    if magic == "spaceguy_3d" and version == 7:
        values_per_vertex = 8
    elif magic == "spaceguy_3d" and version >= 5:
        values_per_vertex = 9
    else:
        values_per_vertex = 6

    for _value in range(vertex_count * values_per_vertex):
        reader.next()

    reader.expect("indices")
    for _value in range(index_count):
        reader.next()

    reader.expect("animations")

    manifest = []

    for _animation in range(animation_count):
        reader.expect("animation")
        name = reader.next()

        if unified:
            reader.expect("type")
            kind = reader.next()
        else:
            kind = default_kind

        manifest.append({"name": name, "kind": kind})

        if kind == "vertex":
            skip_vertex_clip(reader, vertex_count, version)
            if version >= 4:
                skip_attachments(reader)
        elif kind == "transform":
            skip_transform_clip(reader)
        else:
            raise RuntimeError(f"Unsupported animation type '{kind}'")

    return manifest


def order_clips_by_existing_manifest(clips, filepath):
    manifest = read_existing_animation_manifest(filepath)

    if not manifest:
        return sorted(clips, key=lambda clip: (clip["kind"], clip["name"]))

    clips_by_key = {(clip["name"], clip["kind"]): clip for clip in clips}
    ordered = []
    used = set()

    for existing in manifest:
        key = (existing["name"], existing["kind"])
        clip = clips_by_key.get(key)
        if clip is None:
            continue

        ordered.append(clip)
        used.add(key)

    for clip in sorted(clips, key=lambda item: (item["kind"], item["name"])):
        key = (clip["name"], clip["kind"])
        if key not in used:
            ordered.append(clip)

    return ordered


def get_action_key_pose_frames(action, kind):
    frames = set()

    for fcurve in iter_action_fcurves(action):
        if kind == "transform" and fcurve.data_path not in TRANSFORM_PATHS:
            continue

        for keyframe in fcurve.keyframe_points:
            frames.add(int(round(keyframe.co.x)))

    if not frames:
        raise RuntimeError(f"Action '{action.name}' has no keyframes to export.")

    return sorted(frames)


def get_key_pose_frame_range(key_pose_frames):
    return key_pose_frames[0], key_pose_frames[-1]


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


def vec_delta(a, b):
    return (a - b).length


def quat_angle_delta(a, b):
    dot = abs((a.w * b.w) + (a.x * b.x) + (a.y * b.y) + (a.z * b.z))
    dot = min(1.0, max(-1.0, dot))
    return 2.0 * math.acos(dot)


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
    location_ratio = (
        vec_delta(sample["location"], interpolated["location"]) / LOCATION_EPSILON
    )
    rotation_ratio = (
        quat_angle_delta(sample["rotation"], interpolated["rotation"])
        / ROTATION_EPSILON_RADIANS
    )
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


def capture_scene_state(scene, owners):
    state = {"frame": scene.frame_current, "owners": []}

    for owner in owners:
        if owner.animation_data is None:
            state["owners"].append((owner, None, None))
            continue

        action = owner.animation_data.action
        action_slot = None

        if action is not None and hasattr(owner.animation_data, "action_slot"):
            action_slot = owner.animation_data.action_slot

        state["owners"].append((owner, action, action_slot))

    return state


def restore_scene_state(scene, state):
    for owner, action, action_slot in state["owners"]:
        if owner.animation_data is None:
            continue

        owner.animation_data.action = action

        # A slot can only be assigned when an Action is assigned.
        if (
            action is not None
            and action_slot is not None
            and hasattr(owner.animation_data, "action_slot")
        ):
            owner.animation_data.action_slot = action_slot

    scene.frame_set(state["frame"])
    bpy.context.view_layer.update()


def get_attachment_sample(mesh_object, child_object, frame, depsgraph):
    evaluated_mesh = mesh_object.evaluated_get(depsgraph)
    evaluated_child = child_object.evaluated_get(depsgraph)
    child_in_mesh_space = (
        evaluated_mesh.matrix_world.inverted_safe() @ evaluated_child.matrix_world
    )
    location, rotation, scale = child_in_mesh_space.decompose()
    rotation.normalize()

    return {
        "frame": frame,
        "location": location,
        "rotation": rotation,
        "scale": scale,
    }


def write_attachment_sample(file, sample):
    file.write(f"key_pose {sample['frame']}\n")
    file.write("location\n")
    write_vec3(file, sample["location"])
    file.write("rotation_quaternion\n")
    write_quat(file, sample["rotation"])
    file.write("scale\n")
    write_vec3(file, sample["scale"])


def write_vertex_clip(
    file,
    clip,
    obj,
    export_vertices,
    source_vertex_count,
    scene,
    source_loop_count,
    surface_layout,
):
    action = clip["action"]
    owner = clip["owner"]
    key_pose_frames = get_action_key_pose_frames(action, "vertex")
    start_frame, end_frame = get_key_pose_frame_range(key_pose_frames)
    attachments = clip.get("attachments", [])
    attachment_samples = {
        attachment["child"].as_pointer(): [] for attachment in attachments
    }

    assign_action(owner, action)

    file.write("\n")
    file.write(f"animation {clip['name']}\n")
    file.write("type vertex\n")
    loop = bool(action.get("loop", False))
    file.write(f"loop {'true' if loop else 'false'}\n")
    file.write(f"start_frame {start_frame}\n")
    file.write(f"end_frame {end_frame}\n")
    file.write(f"key_pose_count {len(key_pose_frames)}\n")

    for frame in key_pose_frames:
        scene.frame_set(frame)
        bpy.context.view_layer.update()

        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_obj = obj.evaluated_get(depsgraph)
        eval_mesh = eval_obj.to_mesh()

        try:
            validate_surface_layout(eval_obj, eval_mesh, surface_layout)

            if len(eval_mesh.vertices) != source_vertex_count:
                raise RuntimeError(
                    f"Animation {action.name}, key pose frame {frame} has "
                    f"{len(eval_mesh.vertices)} vertices, expected "
                    f"{source_vertex_count}. Animated topology is not supported."
                )

            if len(eval_mesh.loops) != source_loop_count:
                raise RuntimeError(
                    f"Animation {action.name}, key pose frame {frame} changed "
                    "the mesh corner topology."
                )

            file.write(f"\nkey_pose {frame}\n")
            for (
                source_vertex_index,
                loop_index,
                _base_pos,
                _uv,
                _base_normal,
            ) in export_vertices:
                vertex = eval_mesh.vertices[source_vertex_index]
                normal = get_corner_normal(eval_mesh, loop_index)

                write_vec3(file, vertex.co)
                write_vec3(file, normal)
        finally:
            eval_obj.to_mesh_clear()

        for attachment in attachments:
            child = attachment["child"]
            attachment_samples[child.as_pointer()].append(
                get_attachment_sample(obj, child, frame, depsgraph)
            )

    file.write(f"\nattachment_count {len(attachments)}\n")

    for attachment in attachments:
        child = attachment["child"]
        samples = attachment_samples[child.as_pointer()]
        file.write(
            f"attachment {safe_filename(child.name)} "
            f"bone {safe_filename(attachment['bone_name'])}\n"
        )
        file.write(f"key_pose_count {len(samples)}\n")

        for sample in samples:
            file.write("\n")
            write_attachment_sample(file, sample)


def write_transform_clip(file, clip, obj, scene, surface_layout):
    action = clip["action"]
    owner = clip["owner"]
    key_pose_frames = get_action_key_pose_frames(action, "transform")
    authored_key_frames = set(key_pose_frames)
    start_frame, end_frame = get_key_pose_frame_range(key_pose_frames)
    sample_frames = range(start_frame, end_frame + 1)

    assign_action(owner, action)

    all_transform_samples = []

    for frame in sample_frames:
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        validate_evaluated_surface(obj, surface_layout)
        all_transform_samples.append(get_transform_sample(action, owner, frame))

    transform_samples = simplify_transform_samples(
        all_transform_samples, authored_key_frames
    )

    file.write("\n")
    file.write(f"animation {clip['name']}\n")
    file.write("type transform\n")
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


def export_spaceguy_3d(filepath, obj=None):
    if obj is None:
        obj = bpy.context.object

    if obj is None or obj.type != "MESH":
        raise RuntimeError("Select a mesh object before exporting.")

    filepath = Path(filepath).resolve()

    if not filepath.parent.is_dir():
        raise RuntimeError(f"Output folder does not exist: {filepath.parent}")

    scene = bpy.context.scene
    clips = collect_export_clips(obj, filepath)

    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()

    try:
        surface_layout = capture_surface_layout(evaluated, mesh)
        source_vertex_count = len(mesh.vertices)
        source_loop_count = len(mesh.loops)

        export_vertices, export_indices = collect_export_geometry(
            mesh,
            surface_layout["uv_name"],
        )
    finally:
        evaluated.to_mesh_clear()

    if not export_indices:
        raise RuntimeError("The mesh has no triangles to export.")

    relative_texture = Path(
        os.path.relpath(
            surface_layout["image_path"],
            start=filepath.parent,
        )
    ).as_posix()

    fps = scene.render.fps / scene.render.fps_base

    owners = []
    seen = set()

    for clip in clips:
        owner = clip["owner"]
        identity = owner.as_pointer()

        if identity not in seen:
            seen.add(identity)
            owners.append(owner)

    state = capture_scene_state(scene, owners)
    temporary_path = None

    try:
        # Write beside the destination so the final replacement stays local.
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=filepath.parent,
            prefix=filepath.name + ".",
            suffix=".tmp",
            delete=False,
        ) as file:
            temporary_path = Path(file.name)

            file.write("spaceguy_3d 7\n")
            file.write(f"object_name {safe_filename(obj.name)}\n")
            file.write(f"texture_path {quote_path(relative_texture)}\n")
            file.write(f"fps {fps:.6f}\n")
            file.write(f"vertex_count {len(export_vertices)}\n")
            file.write(f"index_count {len(export_indices) * 3}\n")
            file.write(f"animation_count {len(clips)}\n")

            file.write("\nvertices\n")
            file.write("# x y z u v nx ny nz\n")

            for _, _, position, uv, normal in export_vertices:
                file.write(
                    f"{position.x:.9f} {position.y:.9f} {position.z:.9f} "
                    f"{uv.x:.9f} {uv.y:.9f} "
                    f"{normal.x:.9f} {normal.y:.9f} {normal.z:.9f}\n"
                )

            file.write("\nindices\n")

            for a, b, c in export_indices:
                file.write(f"{a} {b} {c}\n")

            file.write("\nanimations\n")

            for clip in clips:
                if clip["kind"] == "vertex":
                    write_vertex_clip(
                        file,
                        clip,
                        obj,
                        export_vertices,
                        source_vertex_count,
                        scene,
                        source_loop_count,
                        surface_layout,
                    )
                elif clip["kind"] == "transform":
                    write_transform_clip(
                        file,
                        clip,
                        obj,
                        scene,
                        surface_layout,
                    )
                else:
                    raise RuntimeError(f"Unsupported animation type: {clip['kind']}")

        restore_scene_state(scene, state)
        temporary_path.replace(filepath)

    finally:
        try:
            restore_scene_state(scene, state)
        finally:
            if temporary_path is not None:
                temporary_path.unlink(missing_ok=True)

    print(f"Exported {filepath}")
    print(f"Texture: {relative_texture}")
    print(f"Vertices: {len(export_vertices)}")
    print(f"Indices: {len(export_indices) * 3}")
    print(f"Animations: {len(clips)}")


def safe_filename(name):
    keep = []
    for char in name:
        if char.isalnum() or char in ("_", "-"):
            keep.append(char)
        else:
            keep.append("_")
    return "".join(keep)


# if __name__ == "__main__":
#     blend_dir = Path(bpy.path.abspath("//"))
#     obj = bpy.context.object
#     output_path = blend_dir / f"{safe_filename(obj.name)}.3d"

#     export_spaceguy_3d(output_path, obj)
#

if __name__ == "__main__":
    obj = bpy.context.object

    if obj is None:
        raise RuntimeError("Select a mesh object before exporting.")

    blend_dir = Path(bpy.path.abspath("//"))
    output_path = blend_dir / f"{safe_filename(obj.name)}_v2.3d"

    export_spaceguy_3d(output_path, obj)
