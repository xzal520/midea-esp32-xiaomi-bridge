#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
using std::isfinite;
class String {
 public:
  String()=default;
  String(const char *s):s_(s?s:""){}
  String(const std::string &s):s_(s){}
  template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>
  String(T n):s_(std::to_string(n)){}
  String(double n,unsigned char digits=2) {std::ostringstream out;out<<std::fixed<<std::setprecision(digits)<<n;s_=out.str();}
  size_t length()const{return s_.size();}
  void reserve(size_t capacity){s_.reserve(capacity);}
  const char *c_str()const{return s_.c_str();}
  int indexOf(const char *s)const{const auto pos=s_.find(s);return pos==std::string::npos?-1:int(pos);}
  String &operator+=(const String &s){s_+=s.s_;return *this;}
  friend String operator+(const String &a,const String &b){return a.s_+b.s_;}
  friend bool operator==(const String &a,const String &b){return a.s_==b.s_;}
  friend bool operator!=(const String &a,const String &b){return !(a==b);}
 private:
  std::string s_;
};
class Stream {
 public:
  virtual ~Stream()=default;
  virtual int available()=0;
  virtual int read()=0;
  virtual size_t write(uint8_t)=0;
  void print(const String &s){for(const char *p=s.c_str();*p;++p)write(uint8_t(*p));}
};
