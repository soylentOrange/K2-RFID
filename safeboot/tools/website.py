import gzip
import os
import sys
import shutil
import subprocess

Import("env")


# html-minifier-terser: use a global install, or the one installed for the
# main project's web app (web/node_modules, set up by tools/build_web.py)
def find_minifier():
    exe = "html-minifier-terser.cmd" if sys.platform == "win32" else "html-minifier-terser"
    local = os.path.join("..", "web", "node_modules", ".bin", exe)
    found = shutil.which("html-minifier-terser") or (local if os.path.isfile(local) else None)
    if found is None:
        sys.exit("website.py: html-minifier-terser not found - run 'npm ci' in web/ or 'npm install html-minifier-terser -g'")
    return found


os.makedirs(".pio/embed", exist_ok=True)

for filename in ["website.html"]:
    skip = False
    # comment out next two lines to always rebuild
    if os.path.isfile(".pio/embed/" + filename + ".gz"):
        skip = True

    if skip:
        sys.stderr.write(f"website.py: {filename}.gz already available\n")
        continue

    # use html-minifier-terser to reduce size of html/js/css
    #   see: https://github.com/terser/html-minifier-terser
    subprocess.run(
        [
            find_minifier(),
            "--remove-comments",
            "--minify-css",            
            '{"level":{"1":{"specialComments":0}}}',
            "--minify-js",
            '{"compress":{"drop_console":true},"mangle":{"toplevel":true},"nameCache":{}}',
            "--case-sensitive",
            "--sort-attributes",
            "--sort-class-name",
            "--remove-tag-whitespace",
            "--collapse-whitespace",
            "--conservative-collapse",
            f"embed/{filename}",
            "-o",
            f".pio/embed/{filename}",
        ],
        check=True,
    )

    # gzip the file
    with open(".pio/embed/" + filename, "rb") as inputFile:
        with gzip.open(".pio/embed/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"website.py: gzip '.pio/embed/{filename}' to '.pio/embed/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # Delete temporary minified html
    os.remove(".pio/embed/" + filename)
