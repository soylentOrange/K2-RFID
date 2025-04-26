import gzip
import os
import sys
import subprocess

Import("env") # pyright: ignore [reportUndefinedVariable]

os.makedirs(".pio/embed", exist_ok=True)
html_files = [f for f in os.listdir('assets/embed_html/') if f.endswith('.html')]

for filename in html_files:
    skip = False
    # comment out next four lines to always rebuild
    if os.path.isfile('.pio/embed/' + filename + '.timestamp'):
        with open('.pio/embed/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('assets/embed_html/' + filename) == float(timestampFile.readline()):
                skip = True

    if skip:
        sys.stderr.write(f"compress_embed_html.py: {filename}.gz already available\n")
        continue

    # use html-minifier-terser to reduce size of html/js/css
    # you need to install html-minifier-terser first:
    #   npm install html-minifier-terser -g
    #   see: https://github.com/terser/html-minifier-terser
    # minified website without console
    subprocess.run(
        [
            "html-minifier-terser",
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
            f"assets/embed_html/{filename}",
            "-o",
            f".pio/embed/{filename}",
        ]
    )

    # minified website with console
    # subprocess.run(
    #     [
    #         "html-minifier-terser",
    #         "--remove-comments",
    #         "--minify-css",
    #         '{"level":{"1":{"specialComments":0}}}',
    #         "--minify-js",
    #         '{"compress":{"drop_console":false},"mangle":{"toplevel":true},"nameCache":{}}',
    #         "--case-sensitive",
    #         "--sort-attributes",
    #         "--sort-class-name",
    #         "--remove-tag-whitespace",
    #         "--collapse-whitespace",
    #         "--conservative-collapse",
    #         f"assets/embed_html/{filename}",
    #         "-o",
    #         f".pio/embed/{filename}",
    #     ]
    # )

    # gzip the file
    with open(".pio/embed/" + filename, "rb") as inputFile:
        with gzip.open(".pio/embed/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"compress_embed_html.py: gzip '.pio/embed/{filename}' to '.pio/embed/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # Delete temporary minified html
    os.remove(".pio/embed/" + filename)

    # remember timestamp of last change
    with open('.pio/embed/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('assets/embed_html/' + filename)))
