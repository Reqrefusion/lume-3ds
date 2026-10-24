"""Locate actual game implementation units in the organized source tree."""
from pathlib import Path

def application_source(folder,name):
    if Path(name).name!=name:
        raise ValueError('Source basename required')
    path=Path(folder)/name if name=='main.c' else Path(folder)/'game/lume'/name
    if not path.is_file():
        raise ValueError('Game source not found: '+str(path))
    return path
