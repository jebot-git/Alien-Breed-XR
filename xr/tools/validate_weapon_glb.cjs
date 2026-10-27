const fs=require('fs'),path=require('path');
const validator=require(process.argv[2]||'gltf-validator');
(async()=>{
 const dir=path.resolve('xr/assets/weapons');const report={validator:validator.version(),files:[]};
 for(const file of fs.readdirSync(dir).filter(x=>x.endsWith('.glb'))){
  const bytes=fs.readFileSync(path.join(dir,file));
  const result=await validator.validateBytes(new Uint8Array(bytes),{uri:file,maxIssues:100});
  const len=bytes.readUInt32LE(12),g=JSON.parse(bytes.subarray(20,20+len));
  const entry={file,bytes:bytes.length,errors:result.issues.numErrors,warnings:result.issues.numWarnings,messages:result.issues.messages,
   animation_names:g.animations.map(a=>a.name),images:g.images.length,mesh_count:g.meshes.length};
  if(g.images.some(i=>i.uri)||g.buffers.some(b=>b.uri))throw Error('External dependency '+file);
  if(g.nodes.some(n=>(n.name||'').startsWith('REFERENCE')))throw Error('Reference mesh leaked '+file);
  if(!g.animations.some(a=>a.name==='Fire'))throw Error('No Fire animation '+file);
  report.files.push(entry);console.log(file,'errors',entry.errors,'warnings',entry.warnings);
 }
 fs.writeFileSync(path.join(dir,'validation.json'),JSON.stringify(report,null,2)+'\n');
 if(report.files.length!==6||report.files.some(f=>f.errors))process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1});
