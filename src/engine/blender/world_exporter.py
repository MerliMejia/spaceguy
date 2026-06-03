import bpy
from pathlib import Path
from mathutils import Vector


def write_vec3(file, v):
    file.write(f"{v.x:.9f} {v.y:.9f} {v.z:.9f}\n")


def safe_filename(name):
    keep = []
    for char in name:
        if char.isalnum() or char in ("_", "-"):
            keep.append(char)
        else:
            keep.append("_")
    return "".join(keep)


def write_transform(file, obj):
    loc = obj.matrix_world.translation
    rot = obj.matrix_world.to_euler()
    scale = obj.matrix_world.to_scale()

    file.write("position ")
    write_vec3(file, loc)

    file.write("rotation ")
    write_vec3(file, rot)

    file.write("scale ")
    write_vec3(file, scale)


def get_camera_look_direction(camera):
    # Blender cameras look along local -Z.
    direction = camera.matrix_world.to_quaternion() @ Vector((0.0, 0.0, -1.0))
    direction.normalize()
    return direction


def export_spaceguy_world(filepath):
    floor = bpy.data.objects.get("floor")
    if floor is None:
        raise RuntimeError("World export requires an object named 'floor'")

    camera = bpy.data.objects.get("camera")
    if camera is None:
        raise RuntimeError("World export requires an object named 'camera'")

    wizards = bpy.data.collections.get("wizards")
    if wizards is None:
        raise RuntimeError(
            "World export requires a collection named 'wizards'")

    wizard_objects = sorted(
        [obj for obj in wizards.objects if obj.type == "MESH"],
        key=lambda obj: obj.name,
    )
    filepath = Path(filepath)

    with filepath.open("w", encoding="utf-8") as file:
        file.write("spaceguy_world 1\n")
        file.write("\n")

        file.write("floor\n")
        write_transform(file, floor)
        file.write("\n")

        file.write("camera\n")
        write_transform(file, camera)
        file.write("look_direction ")
        write_vec3(file, get_camera_look_direction(camera))
        file.write("\n")

        file.write("wizards\n")
        file.write(f"wizard_count {len(wizard_objects)}\n")
        file.write("# x y z\n")
        for wizard in wizard_objects:
            write_vec3(file, wizard.matrix_world.translation)

    print(f"Exported {filepath}")
    print(f"Wizards: {len(wizard_objects)}")


blend_dir = Path(bpy.path.abspath("//"))
output_path = blend_dir / "world.world"

export_spaceguy_world(output_path)
