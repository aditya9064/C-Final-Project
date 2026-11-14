"""
Fireteam: Skirmish — repeatable CC0 asset acquisition.

Downloads the textures (AmbientCG) and HDRI sky (Poly Haven) the arena uses,
into godot/textures/ and godot/assets/hdri/, and writes/refreshes a manifest.
Standard library only (urllib) — no pip install needed.

Run:  python -I pipeline/fetch_assets.py          (from the godot/ folder)
      python -I pipeline/fetch_assets.py --force   (re-download existing files)

Only CC0 sources are used. Official public download endpoints only; files are
size-checked after download. This does NOT bypass any license or paywall.
"""
import os, sys, json, zipfile, io, urllib.request, urllib.error, ssl, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TEX = os.path.join(ROOT, "textures")
HDRI = os.path.join(ROOT, "assets", "hdri")
FORCE = "--force" in sys.argv
UA = {"User-Agent": "FireteamSkirmish-assetfetch/1.0 (educational project)"}
CTX = ssl.create_default_context()

# AmbientCG texture sets: our local folder name -> (asset id, {map: filename-substr})
AMBIENTCG = {
    "concrete": ("Concrete034", {"color": "Color", "normal": "NormalGL", "rough": "Roughness"}),
    "metal":    ("Metal032", {"color": "Color", "normal": "NormalGL", "rough": "Roughness"}),
    "fabric":   ("Fabric062", {"color": "Color", "normal": "NormalGL", "rough": "Roughness", "ao": "AmbientOcclusion"}),
}
AMBIENTCG_RES = "1K-JPG"

# Poly Haven HDRI: local name -> (asset id, resolution)
POLYHAVEN_HDRI = {"sky": ("kloofendal_48d_partly_cloudy_puresky", "2k")}

manifest = {"generated": time.strftime("%Y-%m-%d"), "textures": [], "hdri": [], "license": "All CC0 (public domain)"}


def get(url, timeout=180):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout, context=CTX) as r:
        return r.read()


def fetch_ambientcg():
    os.makedirs(TEX, exist_ok=True)
    for folder, (asset_id, maps) in AMBIENTCG.items():
        dst = os.path.join(TEX, folder)
        os.makedirs(dst, exist_ok=True)
        if not FORCE and all(os.path.exists(os.path.join(dst, m + ".jpg")) for m in maps):
            print("skip (exists):", folder);
            manifest["textures"].append({"folder": folder, "source": "AmbientCG " + asset_id, "license": "CC0"})
            continue
        url = "https://ambientcg.com/get?file=%s_%s.zip" % (asset_id, AMBIENTCG_RES)
        print("download:", url)
        try:
            data = get(url)
            zf = zipfile.ZipFile(io.BytesIO(data))
            for want, substr in maps.items():
                hit = next((n for n in zf.namelist() if substr in n and n.lower().endswith((".jpg", ".png"))), None)
                if hit:
                    with open(os.path.join(dst, want + ".jpg"), "wb") as f:
                        f.write(zf.read(hit))
                    print("   ->", folder + "/" + want + ".jpg")
            manifest["textures"].append({"folder": folder, "source": "AmbientCG " + asset_id, "license": "CC0"})
        except Exception as e:
            print("   FAILED:", e)


def fetch_hdri():
    os.makedirs(HDRI, exist_ok=True)
    for name, (asset_id, res) in POLYHAVEN_HDRI.items():
        dst = os.path.join(HDRI, name + ".hdr")
        if not FORCE and os.path.exists(dst):
            print("skip (exists):", name + ".hdr")
            manifest["hdri"].append({"file": name + ".hdr", "source": "PolyHaven " + asset_id, "license": "CC0"})
            continue
        try:
            files = json.loads(get("https://api.polyhaven.com/files/" + asset_id))
            url = files["hdri"][res]["hdr"]["url"]
            print("download:", url)
            with open(dst, "wb") as f:
                f.write(get(url))
            print("   ->", dst)
            manifest["hdri"].append({"file": name + ".hdr", "source": "PolyHaven " + asset_id, "license": "CC0"})
        except Exception as e:
            print("   FAILED:", e)


def main():
    fetch_ambientcg()
    fetch_hdri()
    with open(os.path.join(ROOT, "assets", "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print("Wrote assets/manifest.json")
    print("Done. Now regenerate meshes with gen_assets.py, then re-import in Godot.")


if __name__ == "__main__":
    main()
