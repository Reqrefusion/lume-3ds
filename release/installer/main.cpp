// SPDX-License-Identifier: GPL-3.0-only
#include <3ds.h>
#include <minizip/unzip.h>
#include <sys/stat.h>
#include <cerrno>
#include "package.hpp"
using namespace lume_setup;
static const char* original="sdmc:/3ds/lume-setup/Lume.exe";
static const char* package_path="sdmc:/3ds/lume-setup/Lume3DS.l3dp";
static const char* payload="sdmc:/3ds/lume-setup/ready.zip.part";
static const char* output_part="sdmc:/3ds/lume/lume.3dsx.part";
static const char* output="sdmc:/3ds/lume/lume.3dsx";
static bool progress(const char* phase,uint64_t done,uint64_t total){
 if(!aptMainLoop())return false;
 hidScanInput();if(hidKeysDown()&KEY_B)return false;
 static unsigned last=101;static std::string last_phase;unsigned percent=total?unsigned(done*100/total):0;
 if(percent!=last||last_phase!=phase){printf("\x1b[16;1H%-30s\n%3u%%   B: Cancel     \n",phase,percent);last=percent;last_phase=phase;gfxFlushBuffers();gfxSwapBuffers();}
 return true;
}
static void extract(){
 unzFile z=unzOpen(payload);require(z!=nullptr,"Cannot open authenticated ZIP payload.");bool created=false;
 try {
  require(unzLocateFile(z,"lume.3dsx",1)==UNZ_OK,"Game file missing in package.");
  unz_file_info64 info{};require(unzGetCurrentFileInfo64(z,&info,nullptr,0,nullptr,0,nullptr,0)==UNZ_OK,"Invalid game ZIP entry.");
  require(info.uncompressed_size>44&&info.uncompressed_size<536870912,"Invalid game size.");
  require(unzOpenCurrentFile(z)==UNZ_OK,"Cannot open game ZIP entry.");
  File f=open(output_part,"wbx");created=true;std::array<uint8_t,65536> bytes;uint64_t total=0;
  for(;;){int n=unzReadCurrentFile(z,bytes.data(),bytes.size());require(n>=0,"Game decompression failed.");if(!n)break;total+=unsigned(n);require(total<=info.uncompressed_size,"Game exceeds expected size.");write(f.get(),bytes.data(),size_t(n));tick(progress,"Installing game",total,info.uncompressed_size);}
  require(total==info.uncompressed_size,"Incomplete game file.");require(unzCloseCurrentFile(z)==UNZ_OK,"Game CRC verification failed.");
  require(fflush(f.get())==0,"Cannot flush game file.");FILE* raw=f.release();require(fclose(raw)==0,"Cannot close game file.");
  File verify=open(output_part,"rb");uint8_t h[44];read(verify.get(),h,sizeof(h));require(memcmp(h,"3DSX",4)==0,"Invalid game executable.");verify.reset();
  require(!exists(output),"Game already exists; it was not replaced.");require(rename(output_part,output)==0,"Cannot publish installed game.");unzClose(z);
 }catch(...){unzClose(z);if(created)remove(output_part);throw;}
}
int main(){
 gfxInitDefault();consoleInit(GFX_TOP,nullptr);consoleInit(GFX_BOTTOM,nullptr);
 // Both models use the same bounded-memory path. New 3DS may use its faster CPU.
 osSetSpeedupEnable(true);
 printf("Lume 3DS Setup\n\nInstall the ready 3D game using\nyour own original Lume.exe.\n\nRequired folder:\n/3ds/lume-setup/\n  Lume.exe\n  Lume3DS.l3dp\n\nA: Install     START: Exit\n\nNo 3D generation or network access.\nAllow at least 400 MiB free.\n");
 while(aptMainLoop()){
  hidScanInput();u32 keys=hidKeysDown();if(keys&KEY_START)break;
  if(keys&KEY_A){
   bool opened=false;
   try{
    require(!exists(output),"Game already installed. Keep it, or back it up and remove only lume.3dsx to reinstall.");
    require(!exists(payload)&&!exists(output_part),"A previous partial file exists. See docs/INSTALLING.md for safe cleanup.");
    u64 free_bytes=0;require(R_SUCCEEDED(FSUSER_GetFreeBytes(&free_bytes,ARCHIVE_SDMC))&&free_bytes>=400ULL*1024*1024,"At least 400 MiB free SD space is required.");
    require(mkdir("sdmc:/3ds/lume",0777)==0||errno==EEXIST,"Cannot create game folder.");
    unseal(original,package_path,payload,progress);opened=true;extract();remove(payload);opened=false;
    printf("\x1b[20;1HInstalled successfully.\nReturn to Homebrew Launcher\nand start Lume.\nSTART: Exit\n");
   }catch(const std::exception& e){if(opened)remove(payload);printf("\x1b[20;1HInstallation stopped:\n%s\nSTART: Exit / A: Retry\n",e.what());}
  }
  gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
 }
 gfxExit();return 0;
}
