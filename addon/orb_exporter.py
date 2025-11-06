bl_info = {
    "name": "Orb Position Exporter",
    "author": "LE4Game",
    "version": (1, 1, 0),
    "blender": (4, 2, 0),
    "location": "3D View > Object > Export Orb Positions",
    "description": "Export mesh positions to JSON for game engine",
    "warning": "",
    "doc_url": "",
    "category": "Import-Export",
}

import bpy
import json
import os
from bpy.props import StringProperty, BoolProperty
from bpy_extras.io_utils import ExportHelper


class OrbExporterPanel(bpy.types.Panel):
    """Creates a Panel in the 3D viewport N-panel"""
    bl_label = "Orb Exporter"
    bl_idname = "VIEW3D_PT_orb_exporter"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Tool"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.label(text="Export Settings:")

        # Export buttons
        col.operator("export.orb_positions_json", text="Export to JSON", icon='FILE_TICK')
        col.operator("export.orb_positions_cpp", text="Export C++ Code", icon='FILE_TEXT')

        # Info
        col.separator()
        col.label(text=f"Meshes in scene: {len([o for o in bpy.context.scene.objects if o.type == 'MESH'])}")


class ExportOrbPositionsJSON(bpy.types.Operator, ExportHelper):
    """Export orb positions to JSON file"""
    bl_idname = "export.orb_positions_json"
    bl_label = "Export Orb Positions (JSON)"
    bl_options = {'REGISTER', 'UNDO'}

    # ExportHelper settings
    filename_ext = ".json"
    filter_glob: StringProperty(default="*.json", options={'HIDDEN'})

    # Export settings
    include_cpp: BoolProperty(
        name="Also Export C++ Code",
        description="Create an additional .txt file with C++ formatted positions",
        default=True,
    )

    def execute(self, context):
        # Collect all mesh objects
        mesh_objects = [obj for obj in context.scene.objects if obj.type == 'MESH']
        mesh_objects.sort(key=lambda x: x.name)

        orb_positions = []

        for i, obj in enumerate(mesh_objects):
            # Get world position
            world_pos = obj.location

            # Export Blender coordinates as-is (no conversion)
            # Conversion will be done in JsonLoader.cpp
            game_x = round(world_pos.x, 3)
            game_y = round(world_pos.y, 3)
            game_z = round(world_pos.z, 3)

            position_data = {
                "index": i,
                "name": obj.name,
                "position": [game_x, game_y, game_z]
            }

            orb_positions.append(position_data)

        # Create output data
        output_data = {
            "orb_count": len(orb_positions),
            "positions": orb_positions
        }

        # Write JSON file
        with open(self.filepath, 'w', encoding='utf-8') as f:
            json.dump(output_data, f, indent=2, ensure_ascii=False)

        self.report({'INFO'}, f"Exported {len(orb_positions)} positions to {os.path.basename(self.filepath)}")

        # Also export C++ code if requested
        if self.include_cpp:
            cpp_filepath = self.filepath.replace('.json', '_cpp.txt')
            self.export_cpp_format(orb_positions, cpp_filepath)
            self.report({'INFO'}, f"C++ code exported to {os.path.basename(cpp_filepath)}")

        return {'FINISHED'}

    def export_cpp_format(self, orb_positions, filepath):
        """Export positions in C++ format"""
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write("// Orb positions for C++ (copy into GamePlayScene.cpp)\n")
            f.write("// Blender coordinates - will be converted in JsonLoader\n\n")
            f.write("const std::vector<Vector3> orbPositions = {\n")

            for pos in orb_positions:
                x, y, z = pos["position"]
                name = pos["name"]
                # Clean up name for C++ comment
                clean_name = name.encode('ascii', 'replace').decode('ascii')
                f.write(f"    Vector3({x}f, {y}f, {z}f),  // {clean_name}\n")

            f.write("};\n")


class ExportOrbPositionsCPP(bpy.types.Operator, ExportHelper):
    """Export orb positions directly as C++ code"""
    bl_idname = "export.orb_positions_cpp"
    bl_label = "Export Orb Positions (C++)"
    bl_options = {'REGISTER', 'UNDO'}

    # ExportHelper settings
    filename_ext = ".cpp"
    filter_glob: StringProperty(default="*.cpp;*.txt;*.h", options={'HIDDEN'})

    def execute(self, context):
        # Collect all mesh objects
        mesh_objects = [obj for obj in context.scene.objects if obj.type == 'MESH']
        mesh_objects.sort(key=lambda x: x.name)

        with open(self.filepath, 'w', encoding='utf-8') as f:
            f.write("// Orb positions for C++ (copy into GamePlayScene.cpp)\n")
            f.write("// Generated from Blender scene\n")
            f.write(f"// Total objects: {len(mesh_objects)}\n\n")
            f.write("const std::vector<Vector3> orbPositions = {\n")

            for i, obj in enumerate(mesh_objects):
                world_pos = obj.location

                # Export Blender coordinates as-is
                game_x = round(world_pos.x, 3)
                game_y = round(world_pos.y, 3)
                game_z = round(world_pos.z, 3)

                # Clean up name for comment
                clean_name = obj.name.encode('ascii', 'replace').decode('ascii')
                f.write(f"    Vector3({game_x}f, {game_y}f, {game_z}f),  // [{i}] {clean_name}\n")

            f.write("};\n")

        self.report({'INFO'}, f"Exported {len(mesh_objects)} positions to {os.path.basename(self.filepath)}")
        return {'FINISHED'}


class QuickExportOrbPositions(bpy.types.Operator):
    """Quick export to desktop"""
    bl_idname = "export.orb_positions_quick"
    bl_label = "Quick Export to Desktop"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # Export to desktop
        desktop = os.path.expanduser("~/Desktop")
        json_path = os.path.join(desktop, "orb_positions.json")
        cpp_path = os.path.join(desktop, "orb_positions.cpp")

        # Collect all mesh objects
        mesh_objects = [obj for obj in context.scene.objects if obj.type == 'MESH']
        mesh_objects.sort(key=lambda x: x.name)

        orb_positions = []

        for i, obj in enumerate(mesh_objects):
            world_pos = obj.location

            # Export Blender coordinates as-is
            game_x = round(world_pos.x, 3)
            game_y = round(world_pos.y, 3)
            game_z = round(world_pos.z, 3)

            position_data = {
                "index": i,
                "name": obj.name,
                "position": [game_x, game_y, game_z]
            }

            orb_positions.append(position_data)

        # Write JSON
        output_data = {
            "orb_count": len(orb_positions),
            "positions": orb_positions
        }

        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(output_data, f, indent=2, ensure_ascii=False)

        # Write C++
        with open(cpp_path, 'w', encoding='utf-8') as f:
            f.write("// Orb positions for C++ (copy into GamePlayScene.cpp)\n")
            f.write("// Blender coordinates - will be converted in JsonLoader\n")
            f.write("const std::vector<Vector3> orbPositions = {\n")

            for pos in orb_positions:
                x, y, z = pos["position"]
                clean_name = pos["name"].encode('ascii', 'replace').decode('ascii')
                f.write(f"    Vector3({x}f, {y}f, {z}f),  // {clean_name}\n")

            f.write("};\n")

        self.report({'INFO'}, f"Exported {len(orb_positions)} positions to Desktop")
        self.report({'INFO'}, "Files: orb_positions.json, orb_positions.cpp")

        return {'FINISHED'}


def menu_func_export(self, context):
    """Add to File > Export menu"""
    self.layout.operator(ExportOrbPositionsJSON.bl_idname, text="Orb Positions (.json)")


def menu_func_object(self, context):
    """Add to Object menu in 3D view"""
    self.layout.separator()
    self.layout.operator(ExportOrbPositionsJSON.bl_idname, text="Export Orb Positions (JSON)")
    self.layout.operator(ExportOrbPositionsCPP.bl_idname, text="Export Orb Positions (C++)")
    self.layout.operator(QuickExportOrbPositions.bl_idname, text="Quick Export to Desktop")


# Registration
classes = [
    OrbExporterPanel,
    ExportOrbPositionsJSON,
    ExportOrbPositionsCPP,
    QuickExportOrbPositions,
]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    # Add to menus
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)
    bpy.types.VIEW3D_MT_object.append(menu_func_object)

    print("Orb Position Exporter: Registered successfully!")
    print("Access from:")
    print("  - File > Export > Orb Positions (.json)")
    print("  - 3D View > Object menu")
    print("  - 3D View > N-panel > Tool tab > Orb Exporter")


def unregister():
    # Remove from menus
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.types.VIEW3D_MT_object.remove(menu_func_object)

    for cls in classes:
        bpy.utils.unregister_class(cls)

    print("Orb Position Exporter: Unregistered")


if __name__ == "__main__":
    # Unregister if already registered (for reloading)
    try:
        unregister()
    except:
        pass

    register()