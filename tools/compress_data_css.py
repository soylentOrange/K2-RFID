import gzip
import os
import sys
import subprocess

Import("env") # pyright: ignore [reportUndefinedVariable]

os.makedirs(".pio/data", exist_ok=True)
os.makedirs("data", exist_ok=True)

# get all css-files (from assets/data_css)
css_files = [f for f in os.listdir('assets/data_css/') if f.endswith('.css')]
for filename in css_files:
    skip = False
    # comment out next four lines to always rebuild
    if os.path.isfile('.pio/data/' + filename + '.timestamp'):
        with open('.pio/data/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('assets/data_css/' + filename) == float(timestampFile.readline()):
                skip = True

    if skip:
        sys.stderr.write(f"compress_data_css.py: {filename}.gz already available\n")
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
            f"assets/data_css/{filename}",
            "-o",
            f".pio/data/{filename}",
        ]
    )

    # gzip the file
    with open(".pio/data/" + filename, "rb") as inputFile:
        with gzip.open("data/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"compress_data_css.py: gzip '.pio/data/{filename}' to 'data/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # Delete temporary minified css
    os.remove(".pio/data/" + filename)

    # remember timestamp of last change
    with open('.pio/data/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('assets/data_css/' + filename)))
