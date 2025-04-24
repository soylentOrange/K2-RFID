import gzip
import os
import sys

os.makedirs('.pio/embed', exist_ok=True)

# list the files for gzipping here!
# don't forget to list them in platformio.ini as well!
for filename in ['logo_captive.svg',
                 'logo_thingy.svg',
                 'logo_webserial.svg',
                 'apple-touch-icon.png',
                 'favicon.svg',
                 'favicon.ico',
                 'icon_96.png',
                 'icon_192.png',
                 'icon_512.png',
                 'toastify.min.js']:
    skip = False
    if os.path.isfile('.pio/embed/' + filename + '.timestamp'):
        with open('.pio/embed/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('embed/' + filename) == float(timestampFile.readline()):
                skip = True
    if skip:
        sys.stderr.write(f"compress_embed.py: {filename} up to date\n")
        continue
    
    with open('embed/' + filename, 'rb') as inputFile:
        with gzip.open('.pio/embed/' + filename + '.gz', 'wb') as outputFile:            
            sys.stderr.write(f"compress_embed.py: gzip \'embed/{filename}\' to \'.pio/embed/{filename}.gz\'\n")
            outputFile.writelines(inputFile)

    with open('.pio/embed/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('embed/' + filename)))