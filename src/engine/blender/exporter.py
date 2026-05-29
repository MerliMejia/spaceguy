import bpy
from pathlib import Path


def write_vec3(file, v):
    file.write(f"{v.x:.9f} {v.y:.9f} {v.z:.9f}\n")


def get_vertex_color(mesh, vertex_index):
    return (1.0, 1.0, 1.0)


def collect_animation_ranges(scene):
    starts = {}
    ends = {}

    for marker in scene.timeline_markers:
        name = marker.name.strip()

        if name.endswith("_start"):
            clip_name = name[:-len("_start")]
            starts[clip_name] = marker.frame

        elif name.endswith("_end"):
            clip_name = name[:-len("_end")]
            ends[clip_name] = marker.frame

    clips = []

    for clip_name, start_frame in starts.items():
        if clip_name not in ends:
            raise RuntimeError(f"Missing marker: {clip_name}_end")

        end_frame = ends[clip_name]

        if end_frame < start_frame:
            raise RuntimeError(
                f"Invalid animation range for {clip_name}: "
                f"{start_frame} to {end_frame}"
            )

        clips.append((clip_name, start_frame, end_frame))

    if not clips:
        raise RuntimeError(
            "No animation markers found. Expected markers like idle_start and idle_end."
        )

    clips.sort(key=lambda item: item[1])
    return clips


def export_spaceguy_3d(filepath, obj=None):
    if obj is None:
        obj = bpy.context.object

    if obj is None:
        raise RuntimeError("No active object selected")

    if obj.type != "MESH":
        raise RuntimeError(f"Active object must be a mesh, got {obj.type}")

    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()

    clips = collect_animation_ranges(scene)

    eval_obj = obj.evaluated_get(depsgraph)
    eval_mesh = eval_obj.to_mesh()
    eval_mesh.calc_loop_triangles()

    base_vertex_count = len(eval_mesh.vertices)
    triangles = [tuple(tri.vertices) for tri in eval_mesh.loop_triangles]

    base_positions = [v.co.copy() for v in eval_mesh.vertices]
    base_colors = [get_vertex_color(eval_mesh, i) for i in range(base_vertex_count)]

    eval_obj.to_mesh_clear()

    fps = scene.render.fps / scene.render.fps_base
    filepath = Path(filepath)

    original_frame = scene.frame_current

    with filepath.open("w", encoding="utf-8") as file:
        file.write("spaceguy_3d 1\n")
        file.write(f"object_name {obj.name}\n")
        file.write(f"fps {fps:.6f}\n")
        file.write(f"vertex_count {base_vertex_count}\n")
        file.write(f"index_count {len(triangles) * 3}\n")
        file.write(f"animation_count {len(clips)}\n")
        file.write("\n")

        file.write("vertices\n")
        file.write("# x y z r g b\n")
        for pos, color in zip(base_positions, base_colors):
            r, g, b = color
            file.write(
                f"{pos.x:.9f} {pos.y:.9f} {pos.z:.9f} "
                f"{r:.6f} {g:.6f} {b:.6f}\n"
            )

        file.write("\n")
        file.write("indices\n")
        file.write("# i0 i1 i2\n")
        for i0, i1, i2 in triangles:
            file.write(f"{i0} {i1} {i2}\n")

        file.write("\n")
        file.write("animations\n")

        try:
            for clip_name, start_frame, end_frame in clips:
                frame_count = end_frame - start_frame + 1

                file.write("\n")
                file.write(f"animation {clip_name}\n")
                file.write(f"start_frame {start_frame}\n")
                file.write(f"end_frame {end_frame}\n")
                file.write(f"frame_count {frame_count}\n")

                for frame in range(start_frame, end_frame + 1):
                    scene.frame_set(frame)
                    bpy.context.view_layer.update()

                    depsgraph = bpy.context.evaluated_depsgraph_get()
                    eval_obj = obj.evaluated_get(depsgraph)
                    eval_mesh = eval_obj.to_mesh()

                    if len(eval_mesh.vertices) != base_vertex_count:
                        eval_obj.to_mesh_clear()
                        raise RuntimeError(
                            f"Animation {clip_name}, frame {frame} has "
                            f"{len(eval_mesh.vertices)} vertices, expected "
                            f"{base_vertex_count}. Animated topology is not supported."
                        )

                    file.write(f"\nframe {frame}\n")
                    for vertex in eval_mesh.vertices:
                        write_vec3(file, vertex.co)

                    eval_obj.to_mesh_clear()

        finally:
            scene.frame_set(original_frame)

    print(f"Exported {filepath}")
    print("Animations:")
    for clip_name, start_frame, end_frame in clips:
        print(f"  {clip_name}: {start_frame} -> {end_frame}")


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