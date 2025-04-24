import gzip
import os
import sys
import subprocess

Import("env") # pyright: ignore [reportUndefinedVariable]

os.makedirs(".pio/embed", exist_ok=True)

for filename in ['toastify.css']:
    skip = False
    # comment out next four lines to always rebuild
    if os.path.isfile('.pio/embed/' + filename + '.timestamp'):
        with open('.pio/embed/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('embed/' + filename) == float(timestampFile.readline()):
                skip = True

    if skip:
        sys.stderr.write(f"compress_css.py: {filename}.gz already available\n")
        continue

    # use clean-css to reduce size css
    # you need to install clean-css first:
    #   npm install clean-css-cli -g
    #   see: https://github.com/clean-css/clean-css
    subprocess.run(
        [
            "cleancss",
            "-O1",
            "specialComments:0",
            f"embed/{filename}",
            "-o",
            f".pio/embed/{filename}",
        ]
    )

    # gzip the file
    with open(".pio/embed/" + filename, "rb") as inputFile:
        with gzip.open(".pio/embed/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"compress_css.py: gzip '.pio/embed/{filename}' to '.pio/embed/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # Delete temporary minified css
    os.remove(".pio/embed/" + filename)

    # remember timestamp of last change
    with open('.pio/embed/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('embed/' + filename)))
