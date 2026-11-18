@tool
extends EditorScenePostImport


func _post_import(scene: Node) -> Object:
	_fix_meshes(scene)
	return scene


func _fix_meshes(node: Node) -> void:
	if node is MeshInstance3D and node.mesh is ArrayMesh:
		var source: ArrayMesh = node.mesh
		var output := ArrayMesh.new()
		output.resource_name = source.resource_name
		for surface in range(source.get_surface_count()):
			var arrays := source.surface_get_arrays(surface)
			var tool := SurfaceTool.new()
			tool.create_from(source, surface)
			if arrays[Mesh.ARRAY_NORMAL] == null or arrays[Mesh.ARRAY_NORMAL].is_empty():
				tool.generate_normals()
			if arrays[Mesh.ARRAY_TANGENT] == null or arrays[Mesh.ARRAY_TANGENT].is_empty():
				tool.generate_tangents()
			tool.commit(output)
			output.surface_set_material(surface, source.surface_get_material(surface))
		node.mesh = output
	for child in node.get_children():
		_fix_meshes(child)
