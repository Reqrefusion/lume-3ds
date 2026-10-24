"""Compile checked-in Java exporters when their sources change.

A portable JRE need not contain javac. The pinned Eclipse compiler runs on
that same JRE; its artifact is verified before use. No global Java install.
"""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile,shutil
URL='https://repo.maven.apache.org/maven2/org/eclipse/jdt/ecj/3.33.0/ecj-3.33.0.jar'
SHA='f7686c4960cf70c2ebc5c500a73a8cfc04541b730c18f1c5c21329889b137f45'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ensure(root,java,jar,download):
 root=Path(root);source=root/'scripts/java';base=root/'tools/java-compiler';state=base/'classes.json'
 inputs={'sources':{p.name:digest(p) for p in sorted(source.glob('*.java'))},'java':digest(java),'ffdec':digest(jar),'compiler':SHA}
 try:old=json.loads(state.read_text(encoding='utf-8'))
 except (OSError,ValueError):old={}
 if old.get('inputs')==inputs and old.get('outputs') and all((source/n).is_file() and digest(source/n)==h for n,h in old['outputs'].items()):return
 compiler=download(URL,base/'ecj-3.33.0.jar',SHA)
 base.mkdir(parents=True,exist_ok=True)
 classpath=os.pathsep.join(str(p) for p in [source,jar,*sorted((Path(jar).parent/'lib').glob('*.jar'))])
 with tempfile.TemporaryDirectory(prefix='classes_',dir=base) as folder:
  cmd=[str(java),'-Xmx512m','-jar',str(compiler),'-17','-proc:none','-warn:none','-encoding','UTF-8','-cp',classpath,'-d',folder,*map(str,sorted(source.glob('*.java')))]
  result=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
  if result.returncode:raise ValueError('Java exporter compilation failed:\n'+result.stdout)
  outputs={p.name:digest(p) for p in Path(folder).glob('*.class')}
  if not outputs:raise ValueError('Java compiler produced no classes')
  for p in Path(folder).glob('*.class'):shutil.copyfile(p,source/p.name)
 state.write_text(json.dumps({'inputs':inputs,'outputs':outputs},indent=2)+'\n',encoding='utf-8')
 print('Java exporters compiled from current sources:',len(outputs),flush=True)
