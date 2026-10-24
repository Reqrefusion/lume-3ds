"""Prepare the official Large weights at iw3's explicit local-checkpoint path.

The upstream loader intentionally does not auto-download CC-BY-NC weights.
Keep that loader intact; this project's dependency setup supplies the selected
public checkpoint, verifies its published LFS digest, and retains partial data
when the connection fails. No model weights are bundled in this patch.
"""
from pathlib import Path
import hashlib
import os
import time
import urllib.request

REVISION = 'cbbb86a30ce19b5684b7a05155dc7e6cbc7685b9'
FILENAME = 'depth_anything_v2_vitl.pth'
SIZE = 1341395338
SHA256 = 'a7ea19fa0ed99244e67b624c72b8580b7e9553043245905be58796a608eb9345'
URL = ('https://huggingface.co/depth-anything/Depth-Anything-V2-Large/'
       'resolve/' + REVISION + '/' + FILENAME + '?download=true')

def digest(path):
    # The checkpoint is 1.34 GB; hashing must not allocate its full contents.
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def valid(path):
    return path.is_file() and path.stat().st_size == SIZE and digest(path) == SHA256

def ensure_checkpoint(repo):
    target = Path(repo) / 'iw3/pretrained_models/hub/checkpoints' / FILENAME
    target.parent.mkdir(parents=True, exist_ok=True)
    if valid(target):
        print('Any V2 Large model verified.', flush=True)
        return target
    if target.exists():
        raise ValueError('Existing model SHA256 mismatch; '
                         'not overwritten: ' + str(target))
    part = target.with_suffix(target.suffix + '.part')
    if part.exists() and part.stat().st_size >= SIZE:
        if valid(part):
            os.replace(part, target)
            return target
        part.unlink()
    start = part.stat().st_size if part.exists() else 0
    headers = {'User-Agent': 'Lume-local-iw3-producer'}
    if start:
        headers['Range'] = 'bytes=%d-' % start
    print('Downloading Any V2 Large (1341 MB, CC-BY-NC-4.0).', flush=True)
    request = urllib.request.Request(URL, headers=headers)
    with urllib.request.urlopen(request, timeout=90) as response:
        status = response.getcode()
        if status == 206:
            wanted = 'bytes %d-%d/%d' % (start, SIZE - 1, SIZE)
            if response.headers.get('Content-Range') != wanted:
                raise ValueError('Model download range mismatch.')
        elif status == 200:
            # A server may ignore Range. Restart rather than append duplicate data.
            start = 0
        else:
            raise ValueError('Beklenmeyen HTTP yaniti: ' + str(status))
        size = start
        last = time.monotonic()
        with part.open('ab' if start else 'wb') as stream:
            while True:
                block = response.read(1024 * 1024)
                if not block:
                    break
                if size + len(block) > SIZE:
                    raise ValueError('Model response exceeds the expected file size.')
                stream.write(block)
                size += len(block)
                now = time.monotonic()
                if now - last >= 5:
                    print('Model: %.1f%% (%d / %d MB)' %
                          (size * 100 / SIZE, size // 1000000, SIZE // 1000000), flush=True)
                    last = now
    if size != SIZE:
        raise ValueError('Model download incomplete. Run again to resume.')
    print('Verifying model SHA256...', flush=True)
    if digest(part) != SHA256:
        part.unlink()
        raise ValueError('Model SHA256 mismatch; corrupt download removed. Run again.')
    os.replace(part, target)
    print('Any V2 Large model ready.', flush=True)
    return target
