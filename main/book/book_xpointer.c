/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：独立按需扫描XHTML，保存有界同名兄弟与直接文本节点计数；正文偏移复用真实转换器。
 * English: Scan XHTML on demand with bounded same-name siblings and direct text-node counters, reusing production text conversion for offsets.
 * 冻结：按用户要求保留Unicode文本位置；不猜测路径、不执行DTD、不修改书源单实例，未知节点拆分保留上层rp1/百分比回退。
 * Frozen: Preserve Unicode text positions as requested; never guess paths, execute DTDs or alter the singleton source, retaining rp1/percentage fallback for unknown node splitting.
 */
#include "book_xpointer.h"
#include "html_text.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEPTH 32
#define SIBLINGS 32
#define NAME 24
#define TEXT_NODE_MAX 8192u
typedef struct { char name[NAME]; uint32_t count; } sibling_t;
typedef struct {
    char name[NAME]; uint32_t index;
    sibling_t siblings[SIBLINGS]; size_t used, begin;
    uint32_t texts;
    bool child;
} node_t;
typedef struct {
    node_t stack[DEPTH]; size_t depth, body_depth;
    char wanted[DEPTH][NAME]; uint32_t indices[DEPTH]; size_t wanted_count;
    size_t raw, begin, end, match_depth;
    uint32_t text_index, text_offset;
    bool encode, found, closed, text, cursor_end;
    char* out; size_t cap, chapter;
} scan_t;
static bool white(char c) { return c==' ' || c=='\n' || c=='\r' || c=='\t'; }
static bool name_char(char c) { return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_'; }
static bool number(const char** p, uint32_t* out) {
    const char* at=*p; uint32_t n=0;
    if (*at<'0'||*at>'9') return false;
    do { unsigned d=(unsigned)(*at-'0'); if(n>(UINT32_MAX-d)/10) return false; n=n*10+d; ++at; } while(*at>='0'&&*at<='9');
    *p=at; *out=n; return true;
}
static bool prefix(const char** p, size_t* chapter) {
    const char* start="/body/DocFragment["; size_t n=strlen(start);
    if(strncmp(*p,start,n)) return false;
    *p+=n; uint32_t value;
    if(!number(p,&value)||!value||value>32768||**p!=']') return false;
    ++*p; *chapter=value-1; return true;
}
static bool path(scan_t* s,const char* p) {
    if(!prefix(&p,&s->chapter)) return false;
    while(*p=='/') {
        ++p;
        if(!strncmp(p,"text()",6)) {
            p+=6; s->text=true;s->text_index=1;
            if(*p=='[') { ++p; if(!number(&p,&s->text_index)||!s->text_index||*p++!=']') return false; }
            if(*p=='.') { ++p; if(!number(&p,&s->text_offset)) return false; }
            break;
        }
        if(s->wanted_count==DEPTH) return false;
        size_t at=0;
        while(name_char(*p)) { if(at+1==NAME) return false; s->wanted[s->wanted_count][at++]=*p++; }
        if(!at) return false;
        uint32_t index=1;
        if(*p=='[') { ++p; if(!number(&p,&index)||!index||*p++!=']') return false; }
        s->indices[s->wanted_count++]=index;
    }
    return !*p && s->wanted_count && !strcmp(s->wanted[0],"body") && s->indices[0]==1;
}
bool book_xpointer_chapter(const char* position,size_t* chapter) {
    if(!position||!chapter) return false;
    scan_t* s=heap_caps_calloc(1,sizeof(*s),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s) return false;
    bool ok=path(s,position); if(ok)*chapter=s->chapter; free(s); return ok;
}
static bool paragraph(const char* n) {
    return !strcmp(n,"p") || !strcmp(n,"li") || !strcmp(n,"div") || !strcmp(n,"blockquote") ||
        (n[0]=='h'&&n[1]>='1'&&n[1]<='6'&&!n[2]);
}
static bool block(const char* n) {
    if(paragraph(n))return true;
    const char* names[]={"body","section","article","aside","header","footer","figure","figcaption","nav","address","ul","ol","dl","dt","dd"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)if(!strcmp(n,names[i]))return true;
    return false;
}
static bool void_tag(const char* n) {
    const char* names[]={"img","br","hr","meta","link","input","source","wbr","area","base","col","embed","param"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)if(!strcmp(n,names[i]))return true;
    return false;
}
static bool output(scan_t* s,uint32_t text_index,uint32_t offset) {
    int n=snprintf(s->out,s->cap,"/body/DocFragment[%u]",(unsigned)s->chapter+1);
    if(n<0||(size_t)n>=s->cap)return false;
    size_t used=(size_t)n;
    for(size_t i=s->body_depth;i<s->depth;++i) {
        n=i==s->body_depth?snprintf(s->out+used,s->cap-used,"/body"):
            snprintf(s->out+used,s->cap-used,"/%s[%lu]",s->stack[i].name,(unsigned long)s->stack[i].index);
        if(n<0||(size_t)n>=s->cap-used)return false;
        used+=(size_t)n;
    }
    n=snprintf(s->out+used,s->cap-used,"/text()[%lu].%lu",(unsigned long)text_index,(unsigned long)offset);
    return n>=0&&(size_t)n<s->cap-used;
}
static bool matches(scan_t* s) {
    if(s->body_depth==SIZE_MAX||s->depth-s->body_depth!=s->wanted_count)return false;
    for(size_t i=0;i<s->wanted_count;++i) {
        node_t* n=&s->stack[s->body_depth+i];
        if(strcmp(n->name,s->wanted[i])||n->index!=s->indices[i])return false;
    }
    return true;
}
// 文本偏移计Unicode标量，不计UTF-8字节；仅解释与本机转换器一致的有界实体。
// Text offsets count Unicode scalars rather than UTF-8 bytes; decode only bounded entities understood by the local converter.
static size_t character(const char* h,size_t len,uint32_t* cp,bool* reference) {
    const unsigned char* p=(const unsigned char*)h;
    *reference=false;
    if(!len)return 0;
    if(*p=='&') {
        size_t end=1;while(end<len&&end<=32&&h[end]!=';'&&!white(h[end])&&h[end]!='<'&&h[end]!='&')++end;
        if(end==len||end>32||h[end]!=';')return 0;
        *reference=true;
        static const struct {const char* name;uint32_t cp;} names[]={{"amp",'&'},{"lt",'<'},{"gt",'>'},{"quot",'"'},{"apos",'\''},{"nbsp",0xa0}};
        for(size_t i=0;i<sizeof(names)/sizeof(*names);++i) {
            if(strlen(names[i].name)==end-1&&!memcmp(h+1,names[i].name,end-1)){*cp=names[i].cp;return end+1;}
        }
        if(end<=2||h[1]!='#')return 0;
        size_t at=2;unsigned base=10;
        if(h[at]=='x'||h[at]=='X'){base=16;++at;}
        if(at==end)return 0;
        uint32_t value=0;
        for(;at<end;++at) {
            unsigned char c=p[at];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:255;
            if(d>=base||value>(0x10ffffu-d)/base)return 0;
            value=value*base+d;
        }
        if(!value||(value>=0xd800&&value<=0xdfff))return 0;
        *cp=value;return end+1;
    }
    if(*p&&*p<0x80){*cp=*p;return 1;}
    size_t n=*p>=0xc2&&*p<=0xdf?2:*p>=0xe0&&*p<=0xef?3:*p>=0xf0&&*p<=0xf4?4:0;
    if(!n||n>len)return 0;
    uint32_t v=*p&(0x7f>>n);
    for(size_t i=1;i<n;++i){if((p[i]&0xc0)!=0x80)return 0;v=(v<<6)|(p[i]&63);}
    if((n==2&&v<0x80)||(n==3&&v<0x800)||(n==4&&v<0x10000)||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return 0;
    *cp=v;return n;
}
static bool hidden(scan_t* s) {
    for(size_t i=s->body_depth;i<s->depth;++i) {
        const char* n=s->stack[i].name;
        if(!strcmp(n,"head")||!strcmp(n,"script")||!strcmp(n,"style"))return true;
    }
    return false;
}
static bool text_node(scan_t* s,const char* h,size_t begin,size_t end) {
    if(s->body_depth==SIZE_MAX||!s->depth||hidden(s)||(!s->encode&&!s->text))return true;
    node_t* parent=&s->stack[s->depth-1];
    size_t count=0,raw_count=0,selected=SIZE_MAX;
    bool space=false,only_space=true;
    for(size_t at=begin;at<end;) {
        uint32_t cp;bool reference;
        size_t n=character(h+at,end-at,&cp,&reference);
        if(!n)return false;
        raw_count+=reference?n:1;
        if(raw_count>=TEXT_NODE_MAX)return false;
        bool ascii_space=cp==' '||cp=='\t'||cp=='\r'||cp=='\n';
        bool ws=!reference&&ascii_space;
        if(!ascii_space)only_space=false;
        if(!ws||!space) {
            if(s->encode&&s->raw==at){selected=count;}
            if(!s->encode&&s->text&&matches(s)&&parent->texts+1==s->text_index&&count==s->text_offset)selected=at;
            ++count;
        }
        space=ws;at+=n;
    }
    // KOReader忽略块元素开头的纯空白节点，内联后真正的文本兄弟仍需计数。
    // KOReader discards initial whitespace-only block nodes; real text siblings after inline children still count.
    if(!count||(only_space&&!parent->child&&block(parent->name)))return true;
    uint32_t index=++parent->texts;parent->child=true;
    if(s->encode&&selected!=SIZE_MAX&&!s->found)s->found=output(s,index,(uint32_t)selected);
    if(!s->encode&&s->text&&matches(s)&&index==s->text_index) {
        if(s->text_offset>count)return false;
        s->cursor_end=s->text_offset==count;
        s->raw=s->cursor_end?end:selected;
        s->end=end;s->found=s->closed=s->raw!=SIZE_MAX;
    }
    return true;
}
static bool scan(scan_t* s,const char* h,size_t len) {
    s->body_depth=SIZE_MAX;
    for(size_t pos=0;pos<len;) {
        if(h[pos]!='<') {
            size_t end=pos;while(end<len&&h[end]!='<')++end;
            if(!text_node(s,h,pos,end))return false;
            pos=end;continue;
        }
        if(len-pos>=4&&!memcmp(h+pos,"<!--",4)) {
            size_t end=pos+4; while(len-end>=3&&memcmp(h+end,"-->",3))++end;
            if (len-end < 3) return false;
            pos=end+3;continue;
        }
        size_t at=pos+1; bool closing=at<len&&h[at]=='/'; if(closing)++at;
        if(at>=len)return false;
        if(h[at]=='!') {
            // 只跳过不含内部子集的DOCTYPE，不解析实体或访问外链。
            // Skip DOCTYPE without an internal subset, never interpreting entities or visiting external references.
            if(len-at<8 || memcmp(h+at,"!DOCTYPE",8)) return false;
            char q=0;
            for(;at<len;++at) {char c=h[at];if(q){if(c==q)q=0;}else if(c=='\''||c=='"')q=c;else if(c=='[')return false;else if(c=='>')break;}
            if(at==len)return false;
            pos=at+1;continue;
        }
        if(h[at]=='?') {
            while(at<len&&h[at]!='>')++at;
            if (at == len) return false;
            pos=at+1;continue;
        }
        char name[NAME]={0};size_t used=0;
        while(at<len&&name_char(h[at])) { if(used+1==NAME)return false;char c=h[at++];name[used++]=c>='A'&&c<='Z'?c+32:c; }
        if(!used||at==len||(!white(h[at])&&h[at]!='/'&&h[at]!='>'))return false;
        // 排除可能重建DOM或改变空白解析的结构，源节点不能冒充阅读器修复后的节点。
        // Reject DOM-rebuilt and special-whitespace structures; source nodes cannot stand in for repaired reader nodes.
        if(!strcmp(name,"table")||!strcmp(name,"svg")||!strcmp(name,"math")||!strcmp(name,"ruby")||
            !strcmp(name,"pre")||!strcmp(name,"textarea"))return false;
        char quote=0;size_t end=at;
        for(;end<len;++end) { char c=h[end];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break; }
        if(end==len)return false;
        bool empty=(end>at&&h[end-1]=='/')||void_tag(name);
        if(closing) {
            if(!s->depth||strcmp(name,s->stack[s->depth-1].name))return false;
            if(s->found&&!s->closed&&!s->encode&&s->match_depth==s->depth) { s->end=pos;s->closed=true; }
            if(s->depth-1==s->body_depth)s->body_depth=SIZE_MAX;
            --s->depth;
        } else {
            if(s->depth==DEPTH)return false;
            uint32_t index=1;
            if(s->depth) {
                node_t* parent=&s->stack[s->depth-1];size_t i=0;
                while(i<parent->used&&strcmp(parent->siblings[i].name,name))++i;
                if(i==parent->used){if(i==SIBLINGS)return false;strcpy(parent->siblings[i].name,name);++parent->used;}
                index=++parent->siblings[i].count;
                parent->child=true;
            }
            node_t* node=&s->stack[s->depth++];memset(node,0,sizeof(*node));strcpy(node->name,name);node->index=index;node->begin=end+1;
            if(!strcmp(name,"body")) { if(s->body_depth!=SIZE_MAX)return false;s->body_depth=s->depth-1;node->index=1; }
            if(!s->encode&&!s->text&&!s->found&&matches(s)) {s->found=true;s->begin=end+1;s->match_depth=s->depth;}
            if(empty) { if(s->found&&!s->closed&&!s->encode&&s->match_depth==s->depth){s->end=end+1;s->closed=true;}--s->depth; }
        }
        pos=end+1;
    }
    return !s->depth && s->found && (s->encode||s->closed);
}
bool book_xpointer_encode(const char* html,size_t len,size_t chapter,size_t byte,char* out,size_t cap) {
    if(!html||!out||!cap||chapter>=32768)return false;
    out[0]=0;size_t raw;
    if(!html_text_source_byte(html,len,byte,&raw))return false;
    scan_t* s=heap_caps_calloc(1,sizeof(*s),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!s)return false;
    s->encode=true;s->raw=raw;s->out=out;s->cap=cap;s->chapter=chapter;
    bool ok=scan(s,html,len);free(s);if(!ok)out[0]=0;return ok;
}
bool book_xpointer_decode(const char* html,size_t len,const char* position,size_t* byte) {
    if(!html||!position||!byte)return false;
    scan_t* s=heap_caps_calloc(1,sizeof(*s),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!s)return false;
    bool ok=path(s,position)&&scan(s,html,len);
    if(ok&&s->text) {
        if(s->cursor_end) {
            if(!html_text_visible_byte(html,len,s->raw,len,byte)) {
                html_text_t text={0};ok=html_to_blocks(html,len,&text)==ESP_OK;
                if(ok)*byte=text.len;
                html_text_free(&text);
            }
        } else ok=html_text_visible_byte(html,len,s->raw,s->end,byte);
    } else if(ok)ok=html_text_visible_byte(html,len,s->begin,s->end,byte);
    free(s);return ok;
}
