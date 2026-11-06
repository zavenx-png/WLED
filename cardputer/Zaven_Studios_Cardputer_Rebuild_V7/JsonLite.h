#pragma once
// Small, allocation-free JSON span reader. WLED JSON payloads are parsed as
// substrings without a large ArduinoJson document or any recursive allocations.
namespace ZV7 { namespace Json {
struct Span { int a=-1,b=-1; bool ok() const {return a>=0&&b>=a;} };
inline bool blank(char c){return c==' '||c=='\t'||c=='\n'||c=='\r';}
inline void whitespace(const String& s,int& p){while(p<(int)s.length()&&blank(s[p]))p++;}
inline Span nextValue(const String& s,int& p){
 whitespace(s,p);int start=p;if(p>=(int)s.length())return {};
 char c=s[p++];
 if(c=='"'){bool escaped=false;while(p<(int)s.length()){
   char t=s[p++];if(escaped){escaped=false;continue;}if(t=='\\'){escaped=true;continue;}if(t=='"')break;
 }return {start,p};}
 if(c=='{'||c=='['){int depth=1;bool inString=false,escaped=false;
  while(p<(int)s.length()&&depth){char t=s[p++];if(inString){if(escaped)escaped=false;else if(t=='\\')escaped=true;else if(t=='"')inString=false;}
   else {if(t=='"')inString=true;else if(t=='{'||t=='[')depth++;else if(t=='}'||t==']')depth--;}}
  return depth==0?Span{start,p}:Span{};}
 while(p<(int)s.length()&&s[p]!=','&&s[p]!=']'&&s[p]!='}'&&!blank(s[p]))p++;
 return {start,p};
}
inline Span root(const String& s){int p=0;return nextValue(s,p);}
inline String str(const String& s,Span span){String t;if(!span.ok())return t;if(s[span.a]!='"')return s.substring(span.a,span.b);
 for(int p=span.a+1;p<span.b-1;p++){char c=s[p];if(c=='\\'&&p+1<span.b-1){c=s[++p];if(c=='n'||c=='r'||c=='t')c=' ';else if(c=='u'){p+=4;c='?';}}if((unsigned char)c>=32)t+=c;}
 return t;}
inline int num(const String& s,Span span,int fallback=0){if(!span.ok())return fallback;return s.substring(span.a,span.b).toInt();}
inline bool yes(const String& s,Span span,bool fallback=false){if(!span.ok())return fallback;return s[span.a]=='t'?true:s[span.a]=='f'?false:fallback;}
struct Array {const String& s;int p,end,index=-1;Span val;Array(const String& j,Span sp):s(j),p(sp.a+1),end(sp.b-1){}
 bool advance(){while(p<end){whitespace(s,p);if(p>=end||s[p]==']')return false;if(s[p]==','){p++;continue;}val=nextValue(s,p);index++;return val.ok();}return false;}};
struct Object {const String& s;int p,end;Span val;String key;Object(const String& j,Span sp):s(j),p(sp.a+1),end(sp.b-1){}
 bool advance(){while(p<end){whitespace(s,p);if(p>=end||s[p]=='}')return false;if(s[p]==','){p++;continue;}if(s[p]!='"')return false;Span k=nextValue(s,p);key=str(s,k);whitespace(s,p);if(p>=end||s[p++]!=':')return false;val=nextValue(s,p);return val.ok();}return false;}};
inline Span member(const String& s,Span obj,const char* name){if(!obj.ok()||s[obj.a]!='{')return {};Object it(s,obj);while(it.advance())if(it.key==name)return it.val;return {};}
inline String text(const String& s,Span obj,const char* name,const String& def=""){Span v=member(s,obj,name);return v.ok()?str(s,v):def;}
inline int number(const String& s,Span obj,const char* name,int def=0){return num(s,member(s,obj,name),def);}
inline bool boolean(const String& s,Span obj,const char* name,bool def=false){return yes(s,member(s,obj,name),def);}
inline String escape(const String& t){String out;out.reserve(t.length()+8);for(size_t i=0;i<t.length();i++){char c=t[i];if(c=='"'||c=='\\'){out+='\\';out+=c;}else if((unsigned char)c>=32)out+=c;else out+=' ';}return out;}
}}