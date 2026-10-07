/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实XHTML路径与正文转换器回归；覆盖段内Unicode位置、文本兄弟、空白、实体与失败回退。
 * English: Production XHTML path/text regressions covering intra-paragraph Unicode positions, text siblings, whitespace, entities and fallback.
 */
#include "book_xpointer.h"
#include "html_text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void anchor(const char* html,const char* literal,const char* expected) {
 html_text_t text={0};size_t byte;char out[512];
 assert(html_to_blocks(html,strlen(html),&text)==0);
 const char* at=strstr(text.utf8,literal);assert(at);
 size_t target=(size_t)(at-text.utf8);
 assert(book_xpointer_encode(html,strlen(html),0,target,out,sizeof(out)));
 assert(!strcmp(out,expected));
 assert(book_xpointer_decode(html,strlen(html),out,&byte)&&byte==target);
 html_text_free(&text);
}
int main(void) {
 const char* html="<?xml version='1.0'?><!DOCTYPE html><html><head><title>隐藏</title></head><body><div><h1>标题</h1><p>第一段 &amp; 内容</p><aside>x</aside><p> 第二段 <em>中文🙂</em> 结束</p><p>   </p><p>末段</p></div></body></html>";
 size_t len=strlen(html),chapter,byte;
 html_text_t text={0};assert(html_to_blocks(html,len,&text)==0);
 char out[512];
 size_t second=(size_t)(strstr(text.utf8,"第二段")-text.utf8);
 assert(book_xpointer_encode(html,len,2,second+strlen("第二段 "),out,sizeof(out)));
 assert(!strcmp(out,"/body/DocFragment[3]/body/div[1]/p[2]/em[1]/text()[1].0"));
 assert(book_xpointer_chapter(out,&chapter)&&chapter==2);
 assert(book_xpointer_decode(html,len,out,&byte)&&byte==second+strlen("第二段 "));
 assert(book_xpointer_decode(html,len,"/body/DocFragment[3]/body/div/p[2]",&byte)&&byte==second);
 assert(book_xpointer_decode(html,len,"/body/DocFragment[3]/body/div/p[2]/text()[2].1",&byte));
 assert(!strcmp(text.utf8+byte,"结束\n末段"));
 assert(!book_xpointer_decode(html,len,"/body/DocFragment[3]/body/div/p[2]/text()[2].24",&byte));
 assert(!book_xpointer_decode(html,len,"/body/DocFragment[3]/body/div/p[3]",&byte));
 assert(!book_xpointer_decode(html,len,"/body/DocFragment[3]/body/div/p[9]",&byte));
 assert(!book_xpointer_chapter("/body/DocFragment[0]/body/p[1]",&chapter));
 assert(!book_xpointer_chapter("/body/DocFragment[42949672960]/body",&chapter));
 assert(!book_xpointer_chapter("/body/DocFragment[1]/body/p[-1]",&chapter));
 assert(!book_xpointer_chapter("/body/DocFragment[1]/body/p[1]/text().oops",&chapter));
 assert(!book_xpointer_encode(html,len,2,second,out,8));
 assert(!book_xpointer_encode(html,len,2,text.len,out,sizeof(out)));
 const char* broken="<html><body><p>bad</div></body></html>";
 assert(!book_xpointer_encode(broken,strlen(broken),0,0,out,sizeof(out)));
 const char* table="<html><body><table><tr><td><p>x</p></td></tr></table></body></html>";
 assert(!book_xpointer_encode(table,strlen(table),0,0,out,sizeof(out)));
 const char* entities="<!DOCTYPE html [<!ENTITY x 'hello'>]><html><body><p>&x;</p></body></html>";
 assert(!book_xpointer_encode(entities,strlen(entities),0,0,out,sizeof(out)));
 const char* comments="<html><body><p>one</p><!-- <p>fake</p> --><p><img src='x'/>第二段</p></body></html>";
 assert(book_xpointer_decode(comments,strlen(comments),"/body/DocFragment[1]/body/p[2]/text().0",&byte));
 html_text_t image={0};assert(html_to_blocks(comments,strlen(comments),&image)==0);
 assert(!strcmp(image.utf8+byte,"第二段"));
 assert(!book_xpointer_encode(comments,strlen(comments),0,image.blocks[1].offset,out,sizeof(out)));
 assert(book_xpointer_encode(comments,strlen(comments),0,byte,out,sizeof(out)));
 html_text_free(&image);
 html_text_free(&text);
 const char* mixed="<html><body><p>甲<a id='empty'/>乙<em>中文🙂</em>丙<!-- comment -->丁</p><p>末段</p></body></html>";
 anchor(mixed,"乙","/body/DocFragment[1]/body/p[1]/text()[2].0");
 anchor(mixed,"🙂","/body/DocFragment[1]/body/p[1]/em[1]/text()[1].2");
 anchor(mixed,"丙","/body/DocFragment[1]/body/p[1]/text()[3].0");
 anchor(mixed,"丁","/body/DocFragment[1]/body/p[1]/text()[4].0");
 const char* spaced="<html><body><p> \t甲\r\n\r\n 乙&amp;中&#x1f642;&nbsp;&nbsp;尾</p></body></html>";
 anchor(spaced,"乙","/body/DocFragment[1]/body/p[1]/text()[1].3");
 anchor(spaced,"尾","/body/DocFragment[1]/body/p[1]/text()[1].9");
 anchor("<html><body>直接文字🙂结束</body></html>","结束","/body/DocFragment[1]/body/text()[1].5");
 const char* ignored="<html><body><p>第一</p><script>hidden text</script><style>x</style><p>正确目标</p></body></html>";
 anchor(ignored,"目标","/body/DocFragment[1]/body/p[2]/text()[1].2");
 assert(!book_xpointer_decode(ignored,strlen(ignored),"/body/DocFragment[1]/body/script/text().2",&byte));
 assert(book_xpointer_decode(mixed,strlen(mixed),"/body/DocFragment[1]/body/p[2]/text()[1].2",&byte));
 assert(byte==strlen("甲乙中文🙂丙丁\n末段"));
 assert(!book_xpointer_decode(mixed,strlen(mixed),"/body/DocFragment[1]/body/p[2]/text()[2].0",&byte));
 assert(!book_xpointer_decode(mixed,strlen(mixed),"/body/DocFragment[1]/body/p[2]/text()[1].3",&byte));
 // 长中文段落中每个Unicode字符都应往返同一字节，不再落回段首。
 // Every Unicode character in a long CJK paragraph round-trips to its byte instead of falling back to the paragraph start.
 char long_html[12000]="<html><body><p>";
 for(int i=0;i<700;++i)strcat(long_html,"中文🙂&amp;");
 strcat(long_html,"</p></body></html>");
 assert(html_to_blocks(long_html,strlen(long_html),&text)==0);
 for(size_t at=0;at<text.len;) {
   assert(book_xpointer_encode(long_html,strlen(long_html),0,at,out,sizeof(out)));
   assert(book_xpointer_decode(long_html,strlen(long_html),out,&byte)&&byte==at);
   unsigned char c=(unsigned char)text.utf8[at];at+=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
 }
 html_text_free(&text);
 char* oversized=malloc(8300);assert(oversized);strcpy(oversized,"<html><body><p>");size_t start=strlen(oversized);
 memset(oversized+start,'x',8192);strcpy(oversized+start+8192,"</p></body></html>");
 assert(!book_xpointer_encode(oversized,strlen(oversized),0,8100,out,sizeof(out)));free(oversized);
 const char* unknown="<html><body><p>&eacute;正文</p></body></html>";
 assert(!book_xpointer_encode(unknown,strlen(unknown),0,9,out,sizeof(out)));
 assert(book_xpointer_decode(unknown,strlen(unknown),"/body/DocFragment[1]/body/p",&byte)&&byte==0);
 char refs[9100]="<html><body><p>";
 for(int i=0;i<1700;++i)strcat(refs,"&amp;");
 strcat(refs,"</p></body></html>");
 assert(!book_xpointer_encode(refs,strlen(refs),0,100,out,sizeof(out)));
 const char* pre="<html><body><pre>  pre text</pre></body></html>";
 assert(!book_xpointer_encode(pre,strlen(pre),0,0,out,sizeof(out)));
 puts("xpointer: Unicode/text-node positions, 2800-character long-paragraph round trips, inline siblings, entities/whitespace, legacy elements and invalid/unsupported fallback PASS");
}
