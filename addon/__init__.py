bl_info = {
    "name": "NavMesh Generator",
    "author": "LE4Game",
    "version": (2, 1, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > NavMesh",
    "description": "Generate navigation meshes from ground and walls",
    "category": "Game Engine",
}

import bpy
import bmesh
import json
import math
from mathutils import Vector
from bpy.props import FloatProperty, StringProperty, PointerProperty, CollectionProperty, IntProperty
from bpy.types import Panel, Operator, PropertyGroup, UIList


class WallObjectItem(PropertyGroup):
    """Wall object item for collection"""
    obj: PointerProperty(
        name="Wall Object",
        type=bpy.types.Object
    )


class NavMeshProperties(PropertyGroup):
    """NavMesh generation properties"""
    ground_object: PointerProperty(
        name="Ground Object",
        description="Ground mesh object",
        type=bpy.types.Object
    )

    wall_objects: CollectionProperty(
        type=WallObjectItem
    )

    wall_index: IntProperty()

    agent_radius: FloatProperty(
        name="Agent Radius",
        description="Radius of the AI agent",
        default=0.5,
        min=0.1,
        max=10.0
    )

    agent_height: FloatProperty(
        name="Agent Height",
        description="Height of the AI agent",
        default=2.0,
        min=0.1,
        max=10.0
    )

    cell_size: FloatProperty(
        name="Cell Size",
        description="Grid sampling cell size (larger = faster)",
        default=1.0,
        min=0.1,
        max=5.0
    )

    simplify_export: FloatProperty(
        name="Simplify",
        description="Simplify mesh before export (0.0=none, 1.0=max)",
        default=0.0,
        min=0.0,
        max=1.0
    )

    compact_json: bpy.props.BoolProperty(
        name="Compact JSON",
        description="Export without indentation (smaller file, harder to read)",
        default=False
    )

    export_path: StringProperty(
        name="Export Path",
        description="Path to export navmesh JSON",
        default="//navmesh.json",
        subtype='FILE_PATH'
    )


class NAVMESH_UL_WallList(UIList):
    """UI List for wall objects"""
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if item.obj:
            layout.label(text=item.obj.name, icon='MESH_CUBE')
        else:
            layout.label(text="(None)", icon='ERROR')


class NAVMESH_OT_ImportGLTF(Operator):
    """Import glTF/glb file"""
    bl_idname = "navmesh.import_gltf"
    bl_label = "Import glTF"
    bl_description = "Import glTF 2.0 (.gltf/.glb) file"
    bl_options = {'REGISTER', 'UNDO'}

    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default="*.glb;*.gltf", options={'HIDDEN'})

    def execute(self, context):
        try:
            bpy.ops.import_scene.gltf(filepath=self.filepath)
            self.report({'INFO'}, f"Imported: {self.filepath}")
            return {'FINISHED'}
        except Exception as e:
            self.report({'ERROR'}, f"Import failed: {str(e)}")
            return {'CANCELLED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class NAVMESH_OT_SetGround(Operator):
    """Set selected object as ground"""
    bl_idname = "navmesh.set_ground"
    bl_label = "Set as Ground"
    bl_description = "Set the active object as ground"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        if not obj or obj.type != 'MESH':
            self.report({'ERROR'}, "Please select a mesh object")
            return {'CANCELLED'}

        props = context.scene.navmesh_props
        props.ground_object = obj

        self.report({'INFO'}, f"Ground set: {obj.name}")
        return {'FINISHED'}


class NAVMESH_OT_AddWalls(Operator):
    """Add selected objects as walls"""
    bl_idname = "navmesh.add_walls"
    bl_label = "Add Selected as Walls"
    bl_description = "Add all selected mesh objects to wall list"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.navmesh_props
        selected_objs = [obj for obj in context.selected_objects if obj.type == 'MESH']

        if not selected_objs:
            self.report({'ERROR'}, "No mesh objects selected")
            return {'CANCELLED'}

        added_count = 0
        for obj in selected_objs:
            # Check if already in list
            already_exists = False
            for item in props.wall_objects:
                if item.obj == obj:
                    already_exists = True
                    break

            if not already_exists and obj != props.ground_object:
                item = props.wall_objects.add()
                item.obj = obj
                added_count += 1

        self.report({'INFO'}, f"Added {added_count} walls")
        return {'FINISHED'}


class NAVMESH_OT_RemoveWall(Operator):
    """Remove wall from list"""
    bl_idname = "navmesh.remove_wall"
    bl_label = "Remove Wall"
    bl_description = "Remove selected wall from list"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.navmesh_props
        if 0 <= props.wall_index < len(props.wall_objects):
            props.wall_objects.remove(props.wall_index)
            if props.wall_index > 0:
                props.wall_index -= 1

        return {'FINISHED'}


class NAVMESH_OT_ClearWalls(Operator):
    """Clear all walls"""
    bl_idname = "navmesh.clear_walls"
    bl_label = "Clear All Walls"
    bl_description = "Clear all walls from list"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.navmesh_props
        props.wall_objects.clear()
        self.report({'INFO'}, "All walls cleared")
        return {'FINISHED'}


class NAVMESH_OT_Generate(Operator):
    """Generate navigation mesh"""
    bl_idname = "navmesh.generate"
    bl_label = "Generate NavMesh"
    bl_description = "Generate walkable and blocked navmesh from ground and walls"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.navmesh_props

        # Check ground object
        ground_obj = props.ground_object
        if not ground_obj:
            self.report({'ERROR'}, "Ground object not set")
            return {'CANCELLED'}

        # Get wall objects
        wall_objs = [item.obj for item in props.wall_objects if item.obj]
        if not wall_objs:
            self.report({'WARNING'}, "No wall objects set. Generating navmesh on ground only.")

        # Clear existing navmesh objects
        for o in bpy.data.objects:
            if o.name.startswith("NavMesh_"):
                bpy.data.objects.remove(o, do_unlink=True)

        # Calculate ground bounding box
        bbox_min = Vector((float('inf'), float('inf'), float('inf')))
        bbox_max = Vector((float('-inf'), float('-inf'), float('-inf')))

        for v in ground_obj.data.vertices:
            world_pos = ground_obj.matrix_world @ v.co
            bbox_min.x = min(bbox_min.x, world_pos.x)
            bbox_min.y = min(bbox_min.y, world_pos.y)
            bbox_min.z = min(bbox_min.z, world_pos.z)
            bbox_max.x = max(bbox_max.x, world_pos.x)
            bbox_max.y = max(bbox_max.y, world_pos.y)
            bbox_max.z = max(bbox_max.z, world_pos.z)

        self.report({'INFO'}, f"Ground: {ground_obj.name}, Walls: {len(wall_objs)}")

        # Grid sampling parameters
        cell_size = props.cell_size
        agent_height = props.agent_height

        walkable_points = []
        blocked_points = []

        # Calculate total samples for progress
        grid_x_count = int((bbox_max.x - bbox_min.x) / cell_size) + 1
        grid_y_count = int((bbox_max.y - bbox_min.y) / cell_size) + 1
        total_samples = grid_x_count * grid_y_count
        sample_count = 0

        # Sample grid in X-Y plane
        x = bbox_min.x
        x_idx = 0
        while x <= bbox_max.x:
            y = bbox_min.y
            y_idx = 0
            while y <= bbox_max.y:
                # Progress report every 10%
                sample_count += 1
                if sample_count % max(1, total_samples // 10) == 0:
                    progress = (sample_count / total_samples) * 100
                    self.report({'INFO'}, f"Sampling: {progress:.0f}%")

                # Raycast downward to find ground
                ray_start = Vector((x, y, bbox_max.z + 10.0))
                ray_direction = Vector((0, 0, -1))

                result = context.scene.ray_cast(
                    context.view_layer.depsgraph,
                    ray_start,
                    ray_direction
                )

                hit, location, normal, index, obj, matrix = result

                if hit and obj == ground_obj:
                    # Raycast upward to check for walls/obstacles
                    ray_up_start = location + Vector((0, 0, 0.1))
                    ray_up_direction = Vector((0, 0, 1))

                    result_up = context.scene.ray_cast(
                        context.view_layer.depsgraph,
                        ray_up_start,
                        ray_up_direction,
                        distance=agent_height
                    )

                    hit_up, loc_up, norm_up, idx_up, obj_up, mat_up = result_up

                    # Check if hit object is in wall list
                    is_blocked = False
                    if hit_up and obj_up in wall_objs:
                        is_blocked = True

                    if is_blocked:
                        blocked_points.append(location)
                    else:
                        walkable_points.append(location)

                y += cell_size
                y_idx += 1
            x += cell_size
            x_idx += 1

        if len(walkable_points) == 0:
            self.report({'ERROR'}, "No walkable areas found")
            return {'CANCELLED'}

        self.report({'INFO'}, f"Walkable: {len(walkable_points)}, Blocked: {len(blocked_points)}")

        # Create walkable mesh (blue)
        self.create_navmesh("NavMesh_Walkable", walkable_points, bbox_min, bbox_max, cell_size, (0.2, 0.5, 1.0, 0.7), context)

        # Create blocked mesh (red)
        if len(blocked_points) > 0:
            self.create_navmesh("NavMesh_Blocked", blocked_points, bbox_min, bbox_max, cell_size, (1.0, 0.2, 0.2, 0.7), context)

        self.report({'INFO'}, "NavMesh generation complete")
        return {'FINISHED'}

    def create_navmesh(self, name, points, bbox_min, bbox_max, cell_size, color, context):
        """Create a navmesh object from sample points"""
        mesh = bpy.data.meshes.new(f"{name}_Data")
        navmesh_obj = bpy.data.objects.new(name, mesh)
        context.collection.objects.link(navmesh_obj)

        # Create mesh using BMesh
        bm = bmesh.new()

        # Create vertices
        vert_map = {}
        for i, point in enumerate(points):
            v = bm.verts.new(point)
            vert_map[i] = v

        bm.verts.ensure_lookup_table()

        # Calculate grid dimensions
        grid_x = int((bbox_max.x - bbox_min.x) / cell_size) + 1
        grid_y = int((bbox_max.y - bbox_min.y) / cell_size) + 1

        # Map grid coordinates to vertex indices
        grid_to_vert = {}
        for i, point in enumerate(points):
            gx = int(round((point.x - bbox_min.x) / cell_size))
            gy = int(round((point.y - bbox_min.y) / cell_size))
            grid_to_vert[(gx, gy)] = i

        # Create faces (two triangles per grid cell)
        for gx in range(grid_x - 1):
            for gy in range(grid_y - 1):
                v00 = grid_to_vert.get((gx, gy))
                v10 = grid_to_vert.get((gx + 1, gy))
                v01 = grid_to_vert.get((gx, gy + 1))
                v11 = grid_to_vert.get((gx + 1, gy + 1))

                if v00 is not None and v10 is not None and v01 is not None and v11 is not None:
                    try:
                        bm.faces.new([vert_map[v00], vert_map[v10], vert_map[v11]])
                        bm.faces.new([vert_map[v00], vert_map[v11], vert_map[v01]])
                    except:
                        pass

        # Write mesh data
        bm.to_mesh(mesh)
        bm.free()

        # Create material
        mat = bpy.data.materials.new(name=f"{name}_Material")
        mat.diffuse_color = color
        mat.blend_method = 'BLEND'

        if len(navmesh_obj.data.materials) == 0:
            navmesh_obj.data.materials.append(mat)
        else:
            navmesh_obj.data.materials[0] = mat


class NAVMESH_OT_Export(Operator):
    """Export navmesh to JSON"""
    bl_idname = "navmesh.export"
    bl_label = "Export NavMesh"
    bl_description = "Export walkable navmesh to JSON file"
    bl_options = {'REGISTER'}

    def execute(self, context):
        props = context.scene.navmesh_props

        # Find NavMesh_Walkable object
        navmesh_obj = None
        for obj in bpy.data.objects:
            if obj.name == "NavMesh_Walkable":
                navmesh_obj = obj
                break

        if not navmesh_obj:
            self.report({'ERROR'}, "No walkable navmesh found. Generate navmesh first.")
            return {'CANCELLED'}

        self.report({'INFO'}, "Exporting... (this may take a while)")

        # Simplify mesh if requested
        if props.simplify_export > 0.0:
            self.report({'INFO'}, f"Simplifying mesh (ratio: {props.simplify_export:.2f})...")
            # Duplicate object for simplification
            bpy.ops.object.select_all(action='DESELECT')
            navmesh_obj.select_set(True)
            context.view_layer.objects.active = navmesh_obj
            bpy.ops.object.duplicate()
            navmesh_obj = context.active_object

            # Apply decimate modifier
            mod = navmesh_obj.modifiers.new(name="Decimate", type='DECIMATE')
            mod.ratio = 1.0 - props.simplify_export
            bpy.ops.object.modifier_apply(modifier="Decimate")

        # Get mesh data
        mesh = navmesh_obj.data

        # Export vertices
        vertices = []
        for v in mesh.vertices:
            vertices.append({
                "x": v.co.x,
                "y": v.co.z,  # Blender Z -> Game Y
                "z": v.co.y   # Blender Y -> Game Z
            })

        # Export triangles
        triangles = []
        mesh.calc_loop_triangles()
        for tri in mesh.loop_triangles:
            v0 = mesh.vertices[tri.vertices[0]].co
            v1 = mesh.vertices[tri.vertices[1]].co
            v2 = mesh.vertices[tri.vertices[2]].co
            center = (v0 + v1 + v2) / 3.0

            triangles.append({
                "indices": [tri.vertices[0], tri.vertices[1], tri.vertices[2]],
                "center": {
                    "x": center.x,
                    "y": center.z,
                    "z": center.y
                }
            })

        # Build adjacency
        adjacency = self.build_adjacency(mesh)

        # Create JSON data
        navmesh_data = {
            "version": "1.0",
            "agent_radius": props.agent_radius,
            "agent_height": props.agent_height,
            "vertices": vertices,
            "triangles": triangles,
            "adjacency": adjacency
        }

        # Write to file
        filepath = bpy.path.abspath(props.export_path)
        with open(filepath, 'w') as f:
            if props.compact_json:
                json.dump(navmesh_data, f, separators=(',', ':'))
            else:
                json.dump(navmesh_data, f, indent=2)

        # Clean up temporary simplified object
        if props.simplify_export > 0.0:
            bpy.data.objects.remove(navmesh_obj, do_unlink=True)

        self.report({'INFO'}, f"NavMesh exported: {filepath} ({len(vertices)} verts, {len(triangles)} tris)")
        return {'FINISHED'}

    def build_adjacency(self, mesh):
        """Build triangle adjacency graph (optimized)"""
        adjacency = {}
        edge_to_faces = {}

        mesh.calc_loop_triangles()
        tri_count = len(mesh.loop_triangles)

        # Build edge-to-face mapping (single pass)
        for i, tri in enumerate(mesh.loop_triangles):
            v0, v1, v2 = tri.vertices[0], tri.vertices[1], tri.vertices[2]

            # Use frozenset for faster hashing (order-independent)
            edges = [
                frozenset([v0, v1]),
                frozenset([v1, v2]),
                frozenset([v2, v0])
            ]

            for edge in edges:
                if edge not in edge_to_faces:
                    edge_to_faces[edge] = []
                edge_to_faces[edge].append(i)

        # Build adjacency (single pass)
        for i, tri in enumerate(mesh.loop_triangles):
            v0, v1, v2 = tri.vertices[0], tri.vertices[1], tri.vertices[2]

            edges = [
                frozenset([v0, v1]),
                frozenset([v1, v2]),
                frozenset([v2, v0])
            ]

            neighbors = set()  # Use set to avoid duplicates
            for edge in edges:
                for face_idx in edge_to_faces[edge]:
                    if face_idx != i:
                        neighbors.add(face_idx)

            adjacency[str(i)] = list(neighbors)

        return adjacency


class NAVMESH_OT_Clear(Operator):
    """Clear generated navmesh"""
    bl_idname = "navmesh.clear"
    bl_label = "Clear NavMesh"
    bl_description = "Remove all generated navmesh objects"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # Remove all navmesh objects
        for o in bpy.data.objects:
            if o.name.startswith("NavMesh_"):
                bpy.data.objects.remove(o, do_unlink=True)

        self.report({'INFO'}, "NavMesh cleared")
        return {'FINISHED'}


class NAVMESH_PT_Panel(Panel):
    """NavMesh generator panel"""
    bl_label = "NavMesh Generator"
    bl_idname = "NAVMESH_PT_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "NavMesh"

    def draw(self, context):
        layout = self.layout
        props = context.scene.navmesh_props

        # Step 0: Import glTF
        box = layout.box()
        box.label(text="Step 0: Import glTF Model", icon='IMPORT')
        box.operator("navmesh.import_gltf", icon='FILEBROWSER')

        layout.separator()

        # Step 1: Ground selection
        box = layout.box()
        box.label(text="Step 1: Ground (Ground) Selection", icon='MESH_PLANE')
        box.prop(props, "ground_object", text="Ground Object")
        box.operator("navmesh.set_ground", icon='CURSOR')

        layout.separator()

        # Step 2: Wall selection
        box = layout.box()
        box.label(text="Step 2: Wall (Wall) Selection", icon='MESH_CUBE')
        box.operator("navmesh.add_walls", icon='ADD')

        row = box.row()
        row.template_list("NAVMESH_UL_WallList", "", props, "wall_objects", props, "wall_index", rows=3)

        col = row.column(align=True)
        col.operator("navmesh.remove_wall", icon='REMOVE', text="")
        col.operator("navmesh.clear_walls", icon='TRASH', text="")

        layout.separator()

        # Step 3: Settings
        box = layout.box()
        box.label(text="Step 3: Settings", icon='SETTINGS')
        box.prop(props, "agent_radius", text="Agent Radius")
        box.prop(props, "agent_height", text="Agent Height")
        box.prop(props, "cell_size", text="Cell Size", slider=True)
        box.label(text="Tip: Larger Cell Size = Faster", icon='INFO')

        layout.separator()

        # Step 4: Generate
        box = layout.box()
        box.label(text="Step 4: Generate", icon='PLAY')
        box.operator("navmesh.generate", icon='MESH_GRID', text="Generate NavMesh")
        box.operator("navmesh.clear", icon='TRASH', text="Clear NavMesh")

        layout.separator()

        # Step 5: Export
        box = layout.box()
        box.label(text="Step 5: Export", icon='EXPORT')
        box.prop(props, "export_path", text="Export Path")
        box.prop(props, "simplify_export", text="Simplify (0=none, 1=max)", slider=True)
        box.prop(props, "compact_json", text="Compact JSON (1 line)")
        box.operator("navmesh.export", icon='FILE_TICK', text="Export NavMesh")

        layout.separator()

        # Instructions
        box = layout.box()
        box.label(text="Usage:", icon='QUESTION')
        box.label(text="0. Import glTF model (optional)")
        box.label(text="1. Select ground mesh -> Set as Ground")
        box.label(text="2. Select walls (multiple) -> Add as Walls")
        box.label(text="3. Adjust settings")
        box.label(text="4. Generate NavMesh (Blue=Walkable, Red=Blocked)")
        box.label(text="5. Export NavMesh to JSON")


# Registration
classes = (
    WallObjectItem,
    NavMeshProperties,
    NAVMESH_UL_WallList,
    NAVMESH_OT_ImportGLTF,
    NAVMESH_OT_SetGround,
    NAVMESH_OT_AddWalls,
    NAVMESH_OT_RemoveWall,
    NAVMESH_OT_ClearWalls,
    NAVMESH_OT_Generate,
    NAVMESH_OT_Export,
    NAVMESH_OT_Clear,
    NAVMESH_PT_Panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.navmesh_props = bpy.props.PointerProperty(type=NavMeshProperties)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.navmesh_props


if __name__ == "__main__":
    register()
