// Minimal JUCE-compatible TEST SHIM for SequencerState unit tests only.
// Not included in the VST3 build.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace juce {
template<typename T> T jlimit(T lo, T hi, T v) { return std::clamp(v, lo, hi); }
template<typename T> int roundToInt(T v) { return static_cast<int>(std::round(v)); }
template<typename T> bool isPositiveAndBelow(T index, T upper) { return index >= 0 && index < upper; }
struct String {
 std::string value;
 String() = default;
 String(const char* s) : value(s ? s : "") {}
 String(std::string s) : value(std::move(s)) {}
 String trim() const {
  auto start=value.find_first_not_of(" \t\n\r");
  if(start==std::string::npos) return String{};
  auto end=value.find_last_not_of(" \t\n\r");
  return String(value.substr(start,end-start+1));
 }
 bool isEmpty() const {return value.empty();}
};
struct MemoryOutputStream {
 std::vector<unsigned char> bytes;
 void write(const void* p, size_t n) {auto* b=static_cast<const unsigned char*>(p);bytes.insert(bytes.end(), b,b+n);}
 void writeByte(char v) { bytes.push_back(static_cast<unsigned char>(v)); }
 void writeInt(int v) {auto n=static_cast<uint32_t>(v); for(int i=0;i<4;++i) writeByte(static_cast<char>(n>>(8*i)));}
 void writeFloat(float f) {uint32_t n=0; static_assert(sizeof(n)==sizeof(f)); std::memcpy(&n,&f,sizeof(n));writeInt(static_cast<int>(n));}
 void writeBool(bool x) {writeByte(x?1:0);}
 const void* getData() const { return bytes.data(); }
 size_t getDataSize() const { return bytes.size(); }
};
struct MemoryInputStream {
 const unsigned char* bytes; size_t len, pos=0;
 MemoryInputStream(const void* d, size_t n, bool):bytes(static_cast<const unsigned char*>(d)),len(n){}
 char readByte(){return pos<len?static_cast<char>(bytes[pos++]):0;}
 int readInt(){uint32_t v=0;for(int i=0;i<4;++i) v|=static_cast<uint32_t>(static_cast<unsigned char>(readByte()))<<(8*i);return static_cast<int>(v);}
 float readFloat(){auto raw=static_cast<uint32_t>(readInt()); float f=0; std::memcpy(&f,&raw,4);return f;}
 bool readBool(){return readByte()!=0;}
};
struct Base64 {
 static String toBase64(const void* data, size_t n){
  constexpr char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  auto* p=static_cast<const unsigned char*>(data);std::string out;out.reserve((n+2)/3*4);
  for(size_t i=0;i<n;i+=3){unsigned a=p[i],b=i+1<n?p[i+1]:0,c=i+2<n?p[i+2]:0;out+=abc[a>>2];out+=abc[((a&3)<<4)|(b>>4)];out+=i+1<n?abc[((b&15)<<2)|(c>>6)]:'=';out+=i+2<n?abc[c&63]:'=';}
  return String(out);
 }
 static bool convertFromBase64(MemoryOutputStream& out,const String& s){
  int val=0,bits=-8;for(char c:s.value){if(c=='=')break;auto pos=std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/").find(c);if(pos==std::string::npos)return false;val=(val<<6)|static_cast<int>(pos);bits+=6;if(bits>=0){out.writeByte(static_cast<char>((val>>bits)&255));bits-=8;}}
  return true;
 }
};
}
