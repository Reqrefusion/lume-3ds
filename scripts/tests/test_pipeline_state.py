"""Production retries must preserve last successful files and checkpoints."""
from pathlib import Path
import importlib.util,tempfile,unittest,json,sys
from unittest.mock import patch
from test_fence_flow import ROOT
spec=importlib.util.spec_from_file_location('pipeline_state_test',ROOT/'scripts/exe/state.py');state=importlib.util.module_from_spec(spec);spec.loader.exec_module(state)
class PipelineStateTests(unittest.TestCase):
 def test_verified_skip_and_same_size_corruption(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   root=Path(folder);tasks=state.Tasks(root/'state');dest=root/'result';calls=[]
   def produce(p):calls.append(1);(p/'asset.bin').write_bytes(b'good')
   tasks.run('example',{'v':1},dest,produce);tasks.run('example',{'v':1},dest,produce)
   self.assertEqual(len(calls),1);self.assertEqual(tasks.events[-1]['status'],'skipped')
   (dest/'asset.bin').write_bytes(b'evil')
   tasks.run('example',{'v':1},dest,produce)
   self.assertEqual(len(calls),2);self.assertEqual((dest/'asset.bin').read_bytes(),b'good')
 def test_action_and_empty_result_preserve_old_tree(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   root=Path(folder);tasks=state.Tasks(root/'state');dest=root/'result'
   tasks.run('example',{},dest,lambda p:(p/'asset').write_text('old'))
   previous=(root/'state/example.json').read_bytes()
   def fail(p):(p/'partial').write_text('bad');raise RuntimeError('producer failed')
   for producer in [fail,lambda p:None]:
    with self.assertRaises((RuntimeError,ValueError)):tasks.run('example',{'new':True},dest,producer)
    self.assertEqual((dest/'asset').read_text(),'old');self.assertEqual((root/'state/example.json').read_bytes(),previous)
   self.assertFalse(list(root.glob('result.building-*')))
 def test_checkpoint_write_failure_rolls_back(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   root=Path(folder);tasks=state.Tasks(root/'state');dest=root/'result'
   tasks.run('example',{},dest,lambda p:(p/'asset').write_text('old'))
   previous=(root/'state/example.json').read_bytes();writer=state.write_json
   def disk_failure(path,data):
    if Path(path).name=='example.json':raise OSError('disk unavailable')
    writer(path,data)
   with patch.object(state,'write_json',side_effect=disk_failure):
    with self.assertRaises(OSError):tasks.run('example',{'new':True},dest,lambda p:(p/'asset').write_text('new'))
   self.assertEqual((dest/'asset').read_text(),'old');self.assertEqual((root/'state/example.json').read_bytes(),previous)
   self.assertEqual(tasks.events[-1]['status'],'failed')
 def test_lock_is_exclusive_and_released(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   lock=Path(folder)/'production.lock'
   with state.Lock(lock):
    with self.assertRaises(ValueError):
     with state.Lock(lock):pass
   with state.Lock(lock):pass

 def test_asset_publication_interrupt_restores_tree_and_manifests(self):
  sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'scripts/exe'));sys.path.insert(0,str(ROOT/'build'))
  import full,manifest
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   root=Path(folder);rom=root/'romfs';rom.mkdir(parents=True)
   (rom/'asset').write_bytes(b'old');produced=root/'produced';produced.mkdir();(produced/'asset').write_bytes(b'new')
   names=['assets.sha256.json','source-manifest.json']
   for name in names:(root/name).write_bytes(b'old metadata')
   def interrupt(project):
    (project/names[0]).write_bytes(b'partial new metadata')
    raise KeyboardInterrupt('cancel publication')
   with patch.object(manifest,'save',side_effect=interrupt):
    with self.assertRaises(KeyboardInterrupt):full.publish(root,produced)
   self.assertEqual((rom/'asset').read_bytes(),b'old')
   for name in names:self.assertEqual((root/name).read_bytes(),b'old metadata')

 def test_incremental_asset_interrupt_restores_changed_files(self):
  sys.path.insert(0,str(ROOT/'build'));import manifest
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   root=Path(folder);rom=root/'romfs';rom.mkdir(parents=True)
   (rom/'asset').write_bytes(b'old');generated=root/'generated';generated.mkdir();(generated/'asset').write_bytes(b'new')
   for name in ['assets.sha256.json','source-manifest.json']:(root/name).write_bytes(b'old metadata')
   replace=manifest.os.replace
   def interrupt(src,dst):
    if Path(dst)==root/'source-manifest.json':raise KeyboardInterrupt('cancel incremental commit')
    return replace(src,dst)
   with patch.object(manifest.os,'replace',side_effect=interrupt):
    with self.assertRaises(KeyboardInterrupt):manifest.commit(root,generated)
   self.assertEqual((rom/'asset').read_bytes(),b'old')
   for name in ['assets.sha256.json','source-manifest.json']:self.assertEqual((root/name).read_bytes(),b'old metadata')

 def test_preview_interrupt_restores_previous_directory(self):
  sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'scripts/exe'))
  import production_previews as preview
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   root=Path(folder);dest=root/'preview';dest.mkdir();(dest/'asset').write_bytes(b'old')
   stage=root/'stage';stage.mkdir();(stage/'asset').write_bytes(b'new');replace=preview.os.replace
   def interrupt(src,target):
    if Path(src)==stage:raise KeyboardInterrupt('cancel preview publish')
    return replace(src,target)
   with patch.object(preview.os,'replace',side_effect=interrupt):
    with self.assertRaises(KeyboardInterrupt):preview.commit(stage,dest,{})
   self.assertEqual((dest/'asset').read_bytes(),b'old')
