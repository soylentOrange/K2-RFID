import gzip
import os
import sys

Import("env") # pyright: ignore [reportUndefinedVariable]

os.makedirs("data", exist_ok=True)
os.makedirs(".pio/data", exist_ok=True)

# get all files in assets/data
data_files = [f for f in os.listdir('assets/data/') if os.path.isfile('assets/data/' + f)]

for filename in data_files:
    skip = False
    # comment out next four lines to always rebuild
    if os.path.isfile('.pio/data/' + filename + '.timestamp'):
        with open('.pio/data/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('assets/data/' + filename) == float(timestampFile.readline()):
                skip = True

    if skip:
        sys.stderr.write(f"compress_data.py: {filename}.gz is up to date\n")
        continue
    
    # gzip the file
    with open("assets/data/" + filename, "rb") as inputFile:
        with gzip.open("data/" + filename + ".gz", "wb") as outputFile:
            sys.stderr.write(
                f"compress_data.py: gzip 'assets/data/{filename}' to 'data/{filename}.gz'\n"
            )
            outputFile.writelines(inputFile)

    # remember timestamp of last change
    with open('.pio/data/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('assets/data/' + filename)))
