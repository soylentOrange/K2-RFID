# Build the embedded web pages (PlatformIO pre-script)
#
# - web/          -> web app, built with Vite into a single html file (npm run build:device)
# - assets/embed_html/webserial.html -> minified with html-minifier-terser
#
# Both end up gzipped in .pio/embed/ and are embedded into the firmware.
# Needs Node.js (>= 20.19); the npm dependencies are installed automatically on first use.

import gzip
import os
import shutil
import subprocess
import sys

Import("env")  # pyright: ignore [reportUndefinedVariable]

WEB_DIR = "web"
EMBED_DIR = os.path.join(".pio", "embed")
NPM = "npm.cmd" if sys.platform == "win32" else "npm"


def log(msg):
    sys.stderr.write(f"build_web.py: {msg}\n")


def newest_mtime(*paths):
    newest = 0.0
    for path in paths:
        if os.path.isfile(path):
            newest = max(newest, os.path.getmtime(path))
        for root, dirs, files in os.walk(path):
            dirs[:] = [d for d in dirs if d not in ("node_modules", "dist", "dist-device")]
            for name in files:
                newest = max(newest, os.path.getmtime(os.path.join(root, name)))
    return newest


def up_to_date(target, *sources):
    return os.path.isfile(target) and os.path.getmtime(target) >= newest_mtime(*sources)


def npm(*args):
    if shutil.which(NPM) is None:
        sys.exit("build_web.py: npm not found - please install Node.js (https://nodejs.org/)")
    subprocess.run([NPM, *args], cwd=WEB_DIR, check=True)


def gzip_file(source, target):
    with open(source, "rb") as src, gzip.open(target, "wb", compresslevel=9) as dst:
        shutil.copyfileobj(src, dst)
    log(f"{source} -> {target} ({os.path.getsize(target)} bytes)")


os.makedirs(EMBED_DIR, exist_ok=True)

# install npm dependencies (once, or when the lock file changed)
lock = os.path.join(WEB_DIR, "package-lock.json")
marker = os.path.join(WEB_DIR, "node_modules", ".package-lock.json")
if not up_to_date(marker, lock):
    log("installing npm dependencies...")
    npm("ci", "--no-audit", "--no-fund")

# web app
website_gz = os.path.join(EMBED_DIR, "website.html.gz")
if up_to_date(website_gz, WEB_DIR, "assets/data/site.webmanifest"):
    log("website.html.gz is up to date")
else:
    npm("run", "build:device")
    gzip_file(os.path.join(WEB_DIR, "dist-device", "index.html"), website_gz)

# web serial console
webserial_gz = os.path.join(EMBED_DIR, "webserial.html.gz")
if up_to_date(webserial_gz, "assets/embed_html/webserial.html", os.path.join(WEB_DIR, "minify.config.json")):
    log("webserial.html.gz is up to date")
else:
    npm("run", "build:webserial")
    webserial = os.path.join(EMBED_DIR, "webserial.html")
    gzip_file(webserial, webserial_gz)
    os.remove(webserial)
