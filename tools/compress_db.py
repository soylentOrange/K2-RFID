import gzip
import os
import sys

Import("env") # pyright: ignore [reportUndefinedVariable]

os.makedirs("data", exist_ok=True)
os.makedirs(".pio/assets", exist_ok=True)

# script is only needed for the database, but still...
for filename in ["material_database.json"]:
    skip = False
    # comment out next four lines to always rebuild
    if os.path.isfile('.pio/assets/' + filename + '.timestamp'):
        with open('.pio/assets/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('assets/' + filename) == float(timestampFile.readline()):
                skip = True

    if skip:
        sys.stderr.write(f"compress_db.py: {filename}.gz is up to date\n")
        continue
    
    # gzip the file
    with open("assets/" + filename, "rb") as inputFile:
        with gzip.open("data/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"compress_db.py: gzip 'assets/{filename}' to 'data/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # remember timestamp of last change
    with open('.pio/assets/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('assets/' + filename)))
