"""Run the publication shell with a fake gh function; never access GitHub."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]

class ReleaseWorkflowTests(unittest.TestCase):
 def simulate(self,name,exists=False):
  bash=shutil.which('bash') if os.name!='nt' else str(Path('C:/Program Files/Git/bin/bash.exe'))
  if not bash or not Path(bash).is_file():self.skipTest('Bash unavailable')
  text=(ROOT/'.github/workflows/build-3dsx.yml').read_text()
  body=text.split('        run: |\n',1)[1]
  shell='\n'.join(line[10:] if line.startswith('          ') else line for line in body.splitlines())
  fake=r'''
gh() {
  printf 'CALL' >> calls.txt
  printf '\t%s' "$@" >> calls.txt
  printf '\n' >> calls.txt
  if [[ "$1 $2" == 'release view' ]]; then return "$VIEW_STATUS"; fi
  return 0
}
'''
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);(root/'release').mkdir();(root/'release/lume.3dsx').write_bytes(b'test fixture, never executed')
   script=root/'test.sh';script.write_text(fake+'\n'+shell,encoding='utf-8',newline='\n')
   env=dict(os.environ,RELEASE_NAME=name,GITHUB_REPOSITORY='owner/repo',GITHUB_SHA='a'*40,VIEW_STATUS='0' if exists else '1')
   result=subprocess.run([bash,'test.sh'],cwd=root,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   return [line.split('\t')[1:] for line in (root/'calls.txt').read_text().splitlines()]
 def test_blank_creates_latest_prerelease(self):
  calls=self.simulate('');create=next(c for c in calls if c[:2]==['release','create'])
  self.assertEqual(create[2],'latest');self.assertIn('--prerelease',create);self.assertIn('--latest=false',create)
  self.assertEqual([arg for arg in create if arg.endswith('.3dsx')],['release/lume.3dsx'])
 def test_existing_latest_updates_asset_and_moves_tag(self):
  calls=self.simulate('  ',True)
  upload=next(c for c in calls if c[:2]==['release','upload']);self.assertIn('--clobber',upload)
  edit=next(c for c in calls if c[:2]==['release','edit']);self.assertIn('--prerelease',edit);self.assertEqual(edit[edit.index('--target')+1],'a'*40)
  patch=next(c for c in calls if c[0]=='api');self.assertIn('PATCH',patch);self.assertIn('repos/owner/repo/git/refs/tags/latest',patch);self.assertIn('sha='+'a'*40,patch)
 def test_custom_name_keeps_title_and_does_not_require_version_numbers(self):
  calls=self.simulate('Summer build');create=next(c for c in calls if c[:2]==['release','create'])
  self.assertEqual(create[create.index('--title')+1],'Summer build');self.assertIn('Summer-build',create);self.assertNotIn('--prerelease',create)
 def test_custom_leading_dash_cannot_be_interpreted_as_a_cli_option(self):
  calls=self.simulate('-test');create=next(c for c in calls if c[:2]==['release','create'])
  self.assertEqual(create[create.index('--')+1],'-test')
if __name__=='__main__':unittest.main()