extends SceneTree
## Convert every PNG in the given directories to JPEG (quality 0.92) and remove
## the PNG. Used to keep the QA screenshot evidence small enough to commit
## (1600x900 PNG ~1.5 MB -> JPEG ~250-400 KB) without needing Python/Pillow.
##
## Usage (no project needed):
##   godot --headless -s godot/qa/png_to_jpg.gd -- <dir> [<dir> ...]
func _init() -> void:
	var dirs := OS.get_cmdline_user_args()
	var n := 0
	for d in dirs:
		var da := DirAccess.open(d)
		if da == null:
			push_error("png_to_jpg: cannot open directory " + d)
			continue
		for f in da.get_files():
			if f.get_extension().to_lower() != "png":
				continue
			var src := d.path_join(f)
			var img := Image.load_from_file(src)
			if img == null:
				push_error("png_to_jpg: cannot load " + src)
				continue
			var dst := src.get_basename() + ".jpg"
			if img.save_jpg(dst, 0.92) != OK:
				push_error("png_to_jpg: save failed " + dst)
				continue
			DirAccess.remove_absolute(src)
			n += 1
			print("converted %s -> %s (%d KB)" % [f, dst.get_file(), FileAccess.get_file_as_bytes(dst).size() / 1024])
	print("PNG_TO_JPG done: %d files" % n)
	quit()
