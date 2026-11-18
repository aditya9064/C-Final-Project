"""Import and exercise the C++ extension, then test independent peers."""

import pathlib
import subprocess
import sys
import tempfile
import time


def main():
    project = pathlib.Path(__file__).resolve().parents[1] / "godot"
    engine = [sys.argv[1], "--headless", "--path", str(project)]
    imported = subprocess.run(
        engine + ["--editor", "--import", "--quit"],
        capture_output=True, text=True, timeout=180,
    )
    if imported.returncode or "ERROR:" in imported.stdout + imported.stderr:
        raise RuntimeError("Godot import failed:\n" + imported.stdout + imported.stderr)
    for script, marker in [
        ("session_smoke.gd", "FIRETEAM_GODOT_SMOKE PASS"),
        ("game_smoke.gd", "FIRETEAM_GAME_SMOKE PASS"),
    ]:
        result = subprocess.run(
            engine + ["--script", "res://tests/" + script],
            capture_output=True, text=True, timeout=45,
        )
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        # Godot can exit zero on a script parse error; require the assertion marker.
        if result.returncode or marker not in result.stdout or "ERROR:" in result.stdout + result.stderr:
            raise RuntimeError("Godot integration test failed: " + script)
    command = [
        sys.argv[1], "--headless", "--path", str(project),
        "--script", "res://tests/session_process.gd",
    ]
    with tempfile.TemporaryDirectory(prefix="fireteam-loopback-") as directory:
        log_path = pathlib.Path(directory) / "host.log"
        with log_path.open("w") as log:
            host = subprocess.Popen(command + ["--", "--test-host"], stdout=log,
                                    stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 30
                port = None
                while time.monotonic() < deadline:
                    for line in log_path.read_text().splitlines():
                        if line.startswith("FIRETEAM_HOST_READY "):
                            port = line.split()[-1]
                    if port or host.poll() is not None:
                        break
                    time.sleep(0.05)
                if port is None:
                    raise RuntimeError("Host failed to start:\n" + log_path.read_text())
                client = subprocess.run(
                    command + ["--", "--test-port", port],
                    capture_output=True, text=True, timeout=45,
                )
                print(client.stdout, end="")
                print(client.stderr, end="", file=sys.stderr)
                if client.returncode or "FIRETEAM_PROCESS_CLIENT PASS" not in client.stdout or "ERROR:" in client.stdout + client.stderr:
                    raise RuntimeError("Client process test failed")
                if host.wait(timeout=30):
                    raise RuntimeError("Host process test failed")
                output = log_path.read_text()
                print(output, end="")
                if "FIRETEAM_PROCESS_HOST PASS" not in output or "ERROR:" in output:
                    raise RuntimeError("Host did not complete its assertions")
                print("FIRETEAM_TWO_PROCESS PASS")
            finally:
                if host.poll() is None:
                    host.terminate()
                    try:
                        host.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        host.kill()
                        host.wait()


if __name__ == "__main__":
    main()
