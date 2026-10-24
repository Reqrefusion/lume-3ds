#ifndef LUME_AUDIO_EVENTS_H
#define LUME_AUDIO_EVENTS_H
/* SFE1 is an offline-prepared bank. Keep effects resident so an authored
   envelope never adds SD access or PCM processing to the presentation loop. */
static EffectSample *authored_samples;
static unsigned authored_count;
static uint32_t event_u32(const uint8_t *p) {
 return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void free_authored_events(void) {
 for(unsigned i=0;i<authored_count;i++)if(authored_samples[i].data)linearFree(authored_samples[i].data);
 free(authored_samples);authored_samples=NULL;authored_count=0;
}
static void load_authored_events(void) {
 FILE *index=fopen("romfs:/sfx_events.bin","rb");
 if(!index)return; /* Old SFX2 media remains playable without the new bank. */
 uint8_t head[8],row[8];unsigned bytes=0;
 if(fread(head,1,8,index)!=8||memcmp(head,"SFE1",4))goto fail;
 unsigned count=event_u32(head+4);if(count>256)goto fail;
 authored_samples=calloc(count?count:1,sizeof(*authored_samples));
 if(!authored_samples)goto fail;
 authored_count=count;
 for(unsigned i=0;i<count;i++) {
  if(fread(row,1,8,index)!=8)goto fail;
  unsigned id=row[0]|(unsigned)row[1]<<8,channels=row[2],frames=event_u32(row+4);
  if(id<32768||(channels!=1&&channels!=2)||row[3]||!frames||frames>4*1024*1024/(channels*2))goto fail;
  for(unsigned j=0;j<i;j++)if(authored_samples[j].id==id)goto fail;
  unsigned size=frames*channels*2;
  if(size>16*1024*1024-bytes)goto fail;
  char path[64];snprintf(path,sizeof path,"romfs:/sfx_%04u.pcm",id);
  FILE *f=fopen(path,"rb");if(!f)goto fail;
  void *data=linearAlloc(size);
  bool ok=data&&fread(data,1,size,f)==size&&fgetc(f)==EOF;fclose(f);
  if(!ok){if(data)linearFree(data);goto fail;}
  authored_samples[i]=(EffectSample){.id=id,.data=data,.samples=frames,.channels=channels};
  flush_audio(data,size);bytes+=size;
 }
 if(fgetc(index)!=EOF)goto fail;
 fclose(index);lume_log("SFX authored: %u events, %u bytes",authored_count,bytes);return;
fail:
 fclose(index);free_authored_events();lume_log("SFX authored bank invalid/unavailable");
}
#endif
