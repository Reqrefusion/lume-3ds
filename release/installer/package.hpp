// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "../../third_party/installer/PicoSHA2/picosha2.h"
extern "C" {
#include "../../third_party/installer/tiny-AES-c/aes.h"
}
namespace lume_setup {
using Hash=std::array<uint8_t,32>;
using Sha=picosha2::hash256_one_by_one;
using Progress=bool(*)(const char*,uint64_t,uint64_t);
struct FileCloser { void operator()(FILE* p) const {if(p)fclose(p);} };
using File=std::unique_ptr<FILE,FileCloser>;
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline File open(const char* path,const char* mode){File f(fopen(path,mode));require(bool(f),"Cannot open file. Check paths, free space and write protection.");return f;}
inline bool exists(const char* p){FILE* f=fopen(p,"rb");if(!f)return false;fclose(f);return true;}
inline void read(FILE* f,void* p,size_t n){require(fread(p,1,n,f)==n,"File is incomplete or cannot be read.");}
inline void write(FILE* f,const void* p,size_t n){require(fwrite(p,1,n,f)==n,"Cannot write file. Check free space.");}
inline uint64_t size(FILE* f){require(fseek(f,0,SEEK_END)==0,"Cannot seek file.");long n=ftell(f);require(n>=0,"File is too large.");rewind(f);return uint64_t(n);}
inline Hash finish(Sha& h){h.finish();Hash out;h.get_hash_bytes(out.begin(),out.end());return out;}
inline bool same(const Hash& a,const Hash& b){unsigned d=0;for(size_t i=0;i<a.size();i++)d|=a[i]^b[i];return d==0;}
inline void tick(Progress p,const char* phase,uint64_t done,uint64_t total){if(p)require(p(phase,done,total),"Installation cancelled. Existing game and saves were preserved.");}
struct Hmac {
 Sha inner;std::array<uint8_t,64> outer;
 explicit Hmac(const Hash& key){std::array<uint8_t,64> pad{};std::copy(key.begin(),key.end(),pad.begin());for(unsigned i=0;i<64;i++){outer[i]=pad[i]^0x5c;pad[i]^=0x36;}inner.process(pad.begin(),pad.end());}
 void process(const uint8_t* p,size_t n){inner.process(p,p+n);}
 Hash finish(){Hash h=lume_setup::finish(inner);Sha s;s.process(outer.begin(),outer.end());s.process(h.begin(),h.end());return lume_setup::finish(s);}
};
inline Hash key(const Hash& owner,const uint8_t* salt,const char* label){Hmac h(owner);h.process(salt,16);std::string tag=std::string("Lume3DS/")+label+"/v1";h.process(reinterpret_cast<const uint8_t*>(tag.data()),tag.size());return h.finish();}
inline uint64_t le64(const uint8_t* p){uint64_t n=0;for(unsigned i=0;i<8;i++)n|=uint64_t(p[i])<<(8*i);return n;}
// Authentication completes before a plaintext output file is created.
// Package bytes are read from the same open handle for both passes.
inline void unseal(const char* original,const char* package,const char* output,Progress progress=nullptr){
 require(!exists(output),"Temporary output already exists. Remove it after checking the previous installation.");
 File input=open(package,"rb");uint64_t length=size(input.get());
 require(length>=164&&length<=2147483647ULL&&(length-148)%16==0,"Invalid package size.");
 std::array<uint8_t,116> header;read(input.get(),header.data(),header.size());
 require(memcmp(header.data(),"LUME3DP1\x01\x00\x00\x00",12)==0,"Unsupported package version.");
 uint64_t plain_size=le64(header.data()+76);uint64_t encrypted_size=length-148;
 require(plain_size>0&&plain_size<=2147483648ULL&&plain_size/16*16+16==encrypted_size,"Invalid payload size.");
 Hash expected_original,expected_payload;std::copy_n(header.data()+12,32,expected_original.begin());std::copy_n(header.data()+44,32,expected_payload.begin());
 Hash owner;
 std::array<uint8_t,65536> buffer;
 {
  File original_file=open(original,"rb");uint64_t total=size(original_file.get());
  require(total==21695462,"Unsupported Lume.exe size. Use the supported original Windows version.");
  Sha digest,owner_digest;const char domain[]="Lume3DS/original-file-key/v1";
  owner_digest.process(reinterpret_cast<const uint8_t*>(domain),reinterpret_cast<const uint8_t*>(domain)+sizeof(domain));
  uint64_t done=0;
  while(done<total){size_t n=size_t(std::min<uint64_t>(buffer.size(),total-done));read(original_file.get(),buffer.data(),n);digest.process(buffer.begin(),buffer.begin()+n);owner_digest.process(buffer.begin(),buffer.begin()+n);done+=n;tick(progress,"Checking original",done,total);}
  require(same(finish(digest),expected_original),"Unsupported original Lume.exe. Check the required SHA256 in README.md.");owner=finish(owner_digest);
 }
 Hash enc=key(owner,header.data()+84,"encryption"),mac=key(owner,header.data()+84,"authentication");
 Hmac auth(mac);rewind(input.get());uint64_t done=0;
 while(done<length-32){size_t n=size_t(std::min<uint64_t>(buffer.size(),length-32-done));read(input.get(),buffer.data(),n);auth.process(buffer.data(),n);done+=n;tick(progress,"Verifying package",done,length-32);}
 Hash tag;read(input.get(),tag.data(),32);require(same(auth.finish(),tag),"Package authentication failed. Download the package again.");
 bool created=false;
 try {
  File output_file=open(output,"wbx");created=true;Sha digest;
  require(fseek(input.get(),116,SEEK_SET)==0,"Cannot seek package.");
  AES_ctx aes;AES_init_ctx_iv(&aes,enc.data(),header.data()+100);done=0;uint64_t written=0;
  while(done<encrypted_size){size_t n=size_t(std::min<uint64_t>(buffer.size(),encrypted_size-done));read(input.get(),buffer.data(),n);AES_CBC_decrypt_buffer(&aes,buffer.data(),n);done+=n;size_t useful=n;
   if(done==encrypted_size){unsigned pad=buffer[n-1];require(pad>=1&&pad<=16,"Invalid package padding.");for(unsigned i=0;i<pad;i++)require(buffer[n-1-i]==pad,"Invalid package padding.");useful-=pad;}
   write(output_file.get(),buffer.data(),useful);digest.process(buffer.begin(),buffer.begin()+useful);written+=useful;tick(progress,"Opening ready assets",done,encrypted_size);
  }
  require(fflush(output_file.get())==0,"Cannot flush output.");FILE* raw=output_file.release();require(fclose(raw)==0,"Cannot close output.");
  require(written==plain_size&&same(finish(digest),expected_payload),"Unpacked payload checksum mismatch.");
  std::fill(owner.begin(),owner.end(),0);std::fill(enc.begin(),enc.end(),0);std::fill(mac.begin(),mac.end(),0);memset(&aes,0,sizeof(aes));
 }catch(...){if(created)remove(output);throw;}
}
}
