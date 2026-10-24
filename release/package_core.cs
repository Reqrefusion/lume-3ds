// SPDX-License-Identifier: GPL-3.0-only
// Standard AES-256-CBC + HMAC-SHA256 encrypt-then-MAC; original file is never executed.
using System;
using System.IO;
using System.Text;
using System.Security.Cryptography;
public static class LumePackage {
 const int HeaderSize=116;
 static readonly byte[] Magic=Encoding.ASCII.GetBytes("LUME3DP1");
 static byte[] Digest(string path){using(var f=File.OpenRead(path))using(var h=SHA256.Create())return h.ComputeHash(f);}
 static bool Same(byte[] a,byte[] b){if(a.Length!=b.Length)return false;int d=0;for(int i=0;i<a.Length;i++)d|=a[i]^b[i];return d==0;}
 static byte[] Owner(string path){
  using(var h=SHA256.Create())using(var f=File.OpenRead(path)){
   byte[] domain=Encoding.ASCII.GetBytes("Lume3DS/original-file-key/v1\0");h.TransformBlock(domain,0,domain.Length,domain,0);
   byte[] b=new byte[1024*1024];int n;while((n=f.Read(b,0,b.Length))>0)h.TransformBlock(b,0,n,b,0);
   h.TransformFinalBlock(new byte[0],0,0);return h.Hash;
  }
 }
 static byte[] Key(byte[] owner,byte[] salt,string label){using(var h=new HMACSHA256(owner)){
  byte[] tag=Encoding.ASCII.GetBytes("Lume3DS/"+label+"/v1");byte[] input=new byte[salt.Length+tag.Length];Buffer.BlockCopy(salt,0,input,0,salt.Length);Buffer.BlockCopy(tag,0,input,salt.Length,tag.Length);return h.ComputeHash(input);
 }}
 static byte[] Mac(string path,long bytes,byte[] key){using(var f=File.OpenRead(path))using(var h=new HMACSHA256(key)){
  byte[] b=new byte[1024*1024];while(bytes>0){int n=f.Read(b,0,(int)Math.Min(b.Length,bytes));if(n==0)throw new InvalidDataException("Incomplete package.");h.TransformBlock(b,0,n,b,0);bytes-=n;}h.TransformFinalBlock(new byte[0],0,0);return h.Hash;
 }}
 static byte[] Read(BinaryReader r,int n){byte[] b=r.ReadBytes(n);if(b.Length!=n)throw new InvalidDataException("Incomplete package header.");return b;}
 static void Clear(byte[] b){if(b!=null)Array.Clear(b,0,b.Length);}
 public static string Sha256(string path){return BitConverter.ToString(Digest(path)).Replace("-","").ToLowerInvariant();}
 public static void Seal(string original,string payload,string output){
  if(File.Exists(output))throw new IOException("Package already exists; refusing to overwrite.");
  byte[] owner=null,enc=null,mac=null,salt=new byte[16],iv=new byte[16];string part=output+"."+Guid.NewGuid().ToString("N")+".part";
  try{
   using(var rng=RandomNumberGenerator.Create()){rng.GetBytes(salt);rng.GetBytes(iv);}
   owner=Owner(original);enc=Key(owner,salt,"encryption");mac=Key(owner,salt,"authentication");
   using(var f=new FileStream(part,FileMode.CreateNew,FileAccess.Write)){
    using(var w=new BinaryWriter(f,Encoding.UTF8,true)){w.Write(Magic);w.Write(1);w.Write(Digest(original));w.Write(Digest(payload));w.Write(new FileInfo(payload).Length);w.Write(salt);w.Write(iv);}
    using(var aes=Aes.Create()){aes.KeySize=256;aes.Key=enc;aes.IV=iv;aes.Mode=CipherMode.CBC;aes.Padding=PaddingMode.PKCS7;
     using(var crypt=new CryptoStream(f,aes.CreateEncryptor(),CryptoStreamMode.Write,true))using(var input=File.OpenRead(payload)){input.CopyTo(crypt,1024*1024);crypt.FlushFinalBlock();}
    }
    f.Flush(true);
   }
   byte[] tag=Mac(part,new FileInfo(part).Length,mac);using(var f=new FileStream(part,FileMode.Append,FileAccess.Write)){f.Write(tag,0,tag.Length);f.Flush(true);}File.Move(part,output);
  }finally{Clear(owner);Clear(enc);Clear(mac);if(File.Exists(part))File.Delete(part);}
 }
 public static void Unseal(string original,string package,string output){
  if(File.Exists(output))throw new IOException("Output already exists; refusing to overwrite.");
  byte[] owner=null,enc=null,mac=null;string part=output+"."+Guid.NewGuid().ToString("N")+".part";
  try{
   long length=new FileInfo(package).Length;if(length<HeaderSize+16+32||(length-HeaderSize-32)%16!=0)throw new InvalidDataException("Invalid package length.");
   byte[] expectedOriginal,expectedPayload,salt,iv,tag;long payloadLength;
   using(var f=File.OpenRead(package))using(var r=new BinaryReader(f)){
    if(!Same(Read(r,8),Magic)||r.ReadInt32()!=1)throw new InvalidDataException("Unknown package version.");
    expectedOriginal=Read(r,32);expectedPayload=Read(r,32);payloadLength=r.ReadInt64();salt=Read(r,16);iv=Read(r,16);
    if(payloadLength<1||payloadLength>2147483648L)throw new InvalidDataException("Invalid payload size.");
    f.Position=length-32;tag=Read(r,32);
   }
   if(!Same(Digest(original),expectedOriginal))throw new InvalidDataException("This is not the supported original Lume.exe file.");
   owner=Owner(original);enc=Key(owner,salt,"encryption");mac=Key(owner,salt,"authentication");
   if(!Same(Mac(package,length-32,mac),tag))throw new InvalidDataException("Package authentication failed. Download the package again.");
   using(var outputFile=new FileStream(part,FileMode.CreateNew,FileAccess.Write))using(var aes=Aes.Create()){
    aes.KeySize=256;aes.Key=enc;aes.IV=iv;aes.Mode=CipherMode.CBC;aes.Padding=PaddingMode.PKCS7;
    using(var crypt=new CryptoStream(outputFile,aes.CreateDecryptor(),CryptoStreamMode.Write,true))using(var input=File.OpenRead(package)){
     input.Position=HeaderSize;long remaining=length-HeaderSize-32;byte[] b=new byte[1024*1024];
     while(remaining>0){int n=input.Read(b,0,(int)Math.Min(b.Length,remaining));if(n==0)throw new InvalidDataException("Incomplete package.");crypt.Write(b,0,n);remaining-=n;}crypt.FlushFinalBlock();
    }
    outputFile.Flush(true);
   }
   if(new FileInfo(part).Length!=payloadLength||!Same(Digest(part),expectedPayload))throw new InvalidDataException("Unpacked payload checksum mismatch.");
   File.Move(part,output);
  }finally{Clear(owner);Clear(enc);Clear(mac);if(File.Exists(part))File.Delete(part);}
 }
}
