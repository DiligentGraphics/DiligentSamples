"""Capture and compare native sample images in one headless Chrome process.

Each sample has its own Wasm executable and browser tab. Capture, PNG encoding,
and comparison use SampleApp's existing native implementation, not screenshots.
Only Python's standard library and Chrome/Chromium are required.
"""

import argparse
from datetime import datetime, timezone
import fnmatch
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import quote, unquote, urlsplit
from urllib.request import Request, urlopen
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


def read_manifest(path, selection):
    cases = json.loads(path.read_text(encoding="utf-8"))
    names = set()
    for case in cases:
        name = case["sample"]
        if not re.fullmatch(r"(?:Tutorials|Samples)/[A-Za-z0-9_-]+", name) or name in names:
            raise ValueError(f"Invalid or duplicate sample: {name}")
        names.add(name)
    selected = [case for case in cases if fnmatch.fnmatchcase(case["sample"], selection)]
    if not selected:
        raise ValueError(f"No samples match {selection!r}")
    return selected


def write_results(output, results):
    failures = sum(r["status"] == "failed" for r in results)
    skipped = sum(r["status"] == "skipped" for r in results)
    suite = ET.Element("testsuite", name="SamplesBrowser", tests=str(len(results)),
                       failures=str(failures), skipped=str(skipped))
    for result in results:
        test = ET.SubElement(suite, "testcase", classname="SamplesBrowser", name=result["sample"],
                             time=str(result.get("seconds", 0)))
        if result["status"] != "passed":
            ET.SubElement(test, "skipped" if result["status"] == "skipped" else "failure",
                          message=result["message"])
    ET.ElementTree(suite).write(output / "results.xml", encoding="utf-8", xml_declaration=True)
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


def run(args):
    cases = read_manifest(args.manifest, args.filter)
    exclusions = {} if args.no_exclusions else json.loads((HERE / "exclude.json").read_text(encoding="utf-8"))
    build = args.build_dir.resolve(strict=True)
    references = args.reference_directory.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    browser = args.browser or shutil.which("google-chrome") or shutil.which("chromium")
    if not browser:
        raise RuntimeError("Chrome was not found; specify --browser")
    page_template = (HERE / "run_tests.html").read_text(encoding="utf-8")
    results = []
    states = []
    log_lock = threading.Lock()
    for index, case in enumerate(cases):
        name = Path(case["sample"]).name
        image = Path(case["sample"]) / f"{name}_{args.mode}.png"
        arguments = ["--mode", "wgpu" if args.mode == "wgpu" else "gles",
                     "--width", str(args.width), "--height", str(args.height),
                     "--golden_image_mode", "capture" if args.update_golden_images else "compare_update",
                     "--capture_path", "/capture", "--capture_name", "frame", "--capture_format", "png",
                     "--adapters_dialog", "0", "--break_on_error", "0",
                     "--golden_image_tolerance", str(args.tolerance)] + case.get("arguments", [])
        config = {"id": index, "module": name + ".js", "arguments": arguments,
                  "capture": args.update_golden_images, "mode": args.mode}
        states.append({"case": case, "module": build / case["sample"] / (name + ".js"),
                       "reference": references / image, "image": output / image,
                       "log": output / case["sample"] / "tests.log", "event": threading.Event(),
                       "page": page_template.replace("CONFIG_JSON", json.dumps(config).replace("<", "\\u003c")).encode(),
                       "report": None})

    class Handler(BaseHTTPRequestHandler):
        def end_headers(self):
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            self.send_header("Cache-Control", "no-store")
            super().end_headers()

        def route(self):
            match = re.fullmatch(r"/case/(\d+)(/.*)?", unquote(urlsplit(self.path).path))
            if not match or int(match[1]) >= len(states):
                self.send_error(404)
                return None, None
            return states[int(match[1])], match[2] or ""

        def do_GET(self):
            state, path = self.route()
            if state is None:
                return
            if path == "":
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(state["page"])))
                self.end_headers()
                self.wfile.write(state["page"])
                return
            if path == "/reference":
                file = state["reference"]
            elif path.startswith("/module/"):
                directory = state["module"].parent.resolve()
                file = (directory / path[len("/module/"):]).resolve()
                if not file.is_relative_to(directory):
                    self.send_error(404)
                    return
            else:
                self.send_error(404)
                return
            if not file.is_file():
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", {".js": "text/javascript", ".wasm": "application/wasm",
                                              ".png": "image/png"}.get(file.suffix, "application/octet-stream"))
            self.send_header("Content-Length", str(file.stat().st_size))
            self.end_headers()
            try:
                with file.open("rb") as stream:
                    shutil.copyfileobj(stream, self.wfile)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_POST(self):
            state, path = self.route()
            if state is None:
                return
            try:
                size = int(self.headers.get("Content-Length", "0"))
                if not 0 < size <= 32 * 1024 * 1024:
                    raise ValueError("Invalid report size")
                data = self.rfile.read(size)
                if path == "/image":
                    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
                        raise ValueError("Invalid captured PNG")
                    state["image"].write_bytes(data)
                elif path == "/log":
                    text = ANSI_ESCAPE.sub("", json.loads(data)["text"])
                    with log_lock:
                        print(text, flush=True)
                        with state["log"].open("a", encoding="utf-8") as stream:
                            stream.write(text + "\n")
                elif path == "/result":
                    if state["report"] is None:
                        state["report"] = json.loads(data)
                else:
                    self.send_error(404)
                    return
                self.send_response(204)
                self.end_headers()
                if path == "/result":
                    state["event"].set()
            except (ValueError, KeyError, TypeError) as error:
                self.send_error(400, str(error))

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="diligent-samples-chrome-") as profile, \
                (output / "chrome.log").open("w", encoding="utf-8") as chrome_log:
            command = [str(browser), "--headless=new", "--no-sandbox", "--disable-dev-shm-usage",
                       "--no-first-run", "--no-default-browser-check", "--remote-debugging-port=0",
                       "--force-device-scale-factor=1", "--disable-background-timer-throttling",
                       "--disable-renderer-backgrounding", "--disable-backgrounding-occluded-windows",
                       "--user-data-dir=" + profile, *args.browser_arg, "about:blank"]
            process = subprocess.Popen(command, stdout=chrome_log, stderr=subprocess.STDOUT)
            try:
                port_file = Path(profile) / "DevToolsActivePort"
                deadline = time.monotonic() + 30
                while not port_file.exists():
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError("Chrome did not start; see chrome.log")
                    time.sleep(0.05)
                debug_url = "http://127.0.0.1:" + port_file.read_text().splitlines()[0]

                for index, state in enumerate(states):
                    case = state["case"]
                    result = {"sample": case["sample"], "mode": args.mode}
                    started = time.monotonic()
                    state["log"].parent.mkdir(parents=True, exist_ok=True)
                    state["log"].write_text("", encoding="utf-8")
                    state["image"].unlink(missing_ok=True)
                    saved_reference = state["image"].with_name(state["image"].stem + "_reference.png")
                    saved_reference.unlink(missing_ok=True)
                    reason = case.get("skip", {}).get(args.mode) or exclusions.get(args.mode, {}).get(case["sample"])
                    if reason:
                        result.update(status="skipped", message=reason)
                    elif not state["module"].is_file():
                        result.update(status="failed", message=f"Missing built module: {state['module']}")
                    else:
                        print(f"[ RUN      ] {case['sample']} ({args.mode})", flush=True)
                        target = None
                        try:
                            url = f"http://127.0.0.1:{server.server_port}/case/{index}"
                            with urlopen(Request(debug_url + "/json/new?" + quote(url, safe=":/"), method="PUT"), timeout=10) as response:
                                target = json.load(response)["id"]
                            deadline = time.monotonic() + args.timeout
                            while not state["event"].wait(0.2):
                                if process.poll() is not None:
                                    raise RuntimeError("Chrome exited during the test")
                                if time.monotonic() >= deadline:
                                    raise RuntimeError(f"No capture result within {args.timeout:g} seconds")
                            report = state["report"]
                            result["environment"] = report.get("environment", {})
                            if report.get("fatal"):
                                raise RuntimeError(report["fatal"])
                            if report.get("code") != 0 or report.get("errors"):
                                raise RuntimeError(f"Native capture/comparison failed (exit {report.get('code')}); see tests.log")
                            if not state["image"].is_file():
                                raise RuntimeError("Native capture completed without an image")
                            result.update(status="passed", message="Captured" if args.update_golden_images else "Compared")
                            if args.update_golden_images:
                                state["reference"].parent.mkdir(parents=True, exist_ok=True)
                                shutil.copyfile(state["image"], state["reference"])
                        except (OSError, ValueError, RuntimeError) as error:
                            result.update(status="failed", message=str(error))
                        finally:
                            if target:
                                # Close the tab from outside its JS event loop. This
                                # also recovers from a native abort or blocked sample.
                                try:
                                    urlopen(debug_url + "/json/close/" + target, timeout=10).close()
                                except OSError:
                                    pass
                    result["seconds"] = round(time.monotonic() - started, 3)
                    if result["status"] == "failed" and state["reference"].is_file():
                        shutil.copyfile(state["reference"], saved_reference)
                    results.append(result)
                    print(f"[ {result['status'].upper():8} ] {case['sample']}: {result['message']} ({result['seconds']} s)", flush=True)
                    write_results(output, results)
            finally:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    finally:
        server.shutdown()
        server.server_close()
        server_thread.join()
    passed = sum(r["status"] == "passed" for r in results)
    failed = sum(r["status"] == "failed" for r in results)
    print(f"Browser captures: {passed} passed, {failed} failed, {len(results) - passed - failed} skipped", flush=True)
    if args.update_golden_images and passed:
        metadata = {"capturedAt": datetime.now(timezone.utc).isoformat(),
                    "width": args.width, "height": args.height, "tolerance": args.tolerance,
                    "results": results}
        (references / f"metadata_{args.mode}.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return 1 if failed or not passed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True, help="DiligentSamples build directory")
    parser.add_argument("--reference-directory", type=Path, required=True, help="GoldenImages/Chrome directory")
    parser.add_argument("--output", type=Path, default=Path("SamplesBrowserResults"))
    parser.add_argument("--manifest", type=Path, default=HERE / "manifest.json")
    parser.add_argument("--filter", default="*", help="Sample path glob")
    parser.add_argument("--no-exclusions", action="store_true", help="Retry known failures; unsupported-feature cases remain skipped")
    parser.add_argument("--mode", choices=("wgpu", "webgl"), default="wgpu")
    parser.add_argument("--browser", default=os.environ.get("DILIGENT_BROWSER_EXECUTABLE"))
    parser.add_argument("--browser-arg", action="append", default=[])
    parser.add_argument("--width", type=int, default=512)
    parser.add_argument("--height", type=int, default=512)
    parser.add_argument("--tolerance", type=int, default=2, help="Native per-channel tolerance; no pixels may exceed it")
    parser.add_argument("--timeout", type=float, default=180, help="Per-sample timeout in seconds")
    parser.add_argument("--update-golden-images", action="store_true", help="Explicitly capture new reference PNGs")
    args = parser.parse_args()
    if min(args.width, args.height, args.timeout) <= 0 or not 0 <= args.tolerance <= 255:
        parser.error("Invalid dimensions, timeout, or tolerance")
    return run(args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Browser sample tests failed: {error}", file=sys.stderr)
        sys.exit(1)
