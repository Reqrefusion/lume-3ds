"""Nested C units and exact header/include fingerprints for incremental builds."""
from pathlib import Path
import hashlib,json

def source_units(source):
    source=Path(source)
    return sorted(source.rglob('*.c'))

def object_path(work,relative_source):
    path=Path(relative_source)
    if path.is_absolute() or '..' in path.parts:
        raise ValueError('Invalid source path: '+str(path))
    return Path(work)/path.with_suffix('.o')

def compile_signature(version,flags,source,dependency_roots=()):
    source=Path(source)
    digest=hashlib.sha256()
    digest.update(json.dumps([version,flags],ensure_ascii=False).encode('utf-8'))
    # Include path names as well as bytes: moving or adding a dependency also
    # invalidates old objects. Nested .inc bodies used to be entirely ignored.
    for path in sorted(source.rglob('*')):
        if path.is_file() and path.suffix.lower() in {'.h','.inc'}:
            digest.update(path.relative_to(source).as_posix().encode('utf-8')+b'\0')
            digest.update(hashlib.sha256(path.read_bytes()).digest())
    for root in dependency_roots:
        root=Path(root)
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.suffix.lower() in {'.c','.h','.inc'}:
                digest.update(path.relative_to(root).as_posix().encode('utf-8')+b'\0')
                digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()
