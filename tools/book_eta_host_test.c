/* SPDX-License-Identifier: Apache-2.0
 * 中文：剩余时间有效样本、长书、窗口与溢出边界回归。
 * English: ETA sample validity, long-book formatting, window and overflow regressions.
 */
#include "book_eta.h"
#include <assert.h>
#include <limits.h>
int main(void) {
    book_eta_t eta = {0}; char text[40];
    for (unsigned i=0;i<7;++i) book_eta_sample(&eta,30000,i*1000,(i+1)*1000);
    book_eta_format(&eta,1000000,text,sizeof(text));assert(!text[0]);
    book_eta_sample(&eta,30000,7000,8000);
    book_eta_format(&eta,1998000,text,sizeof(text));assert(!strcmp(text," 剩16时39分"));
    book_eta_format(&eta,2000000,text,sizeof(text));assert(!strcmp(text," 剩16时40分"));
    book_eta_format(&eta,4000000,text,sizeof(text));assert(!strcmp(text," 剩1天9时"));
    book_eta_format(&eta,20000,text,sizeof(text));assert(!strcmp(text," 剩10分"));
    book_eta_format(&eta,1,text,sizeof(text));assert(!strcmp(text," 剩1分"));
    book_eta_format(&eta,0,text,sizeof(text));assert(!strcmp(text," 剩0分"));
    book_eta_sample(&eta,2999,1,2);book_eta_sample(&eta,120001,1,2);
    book_eta_sample(&eta,-1,1,2);book_eta_sample(&eta,30000,2,1);book_eta_sample(&eta,30000,1,1);
    assert(eta.count==8);
    for(unsigned i=0;i<40;++i)book_eta_sample(&eta,60000,0,2000);
    assert(eta.count==32&&eta.total_ms==1920000&&eta.total_bytes==64000);
    book_eta_format(&eta,20000,text,sizeof(text));assert(!strcmp(text," 剩10分"));
    memset(&eta,0,sizeof(eta));
    for(unsigned i=0;i<32;++i)book_eta_sample(&eta,120000,0,UINT32_MAX);
    book_eta_format(&eta,UINT32_MAX,text,sizeof(text));assert(!strcmp(text," 剩2分"));
    book_eta_format(&eta,UINT32_MAX,text,1);assert(!text[0]);
    puts("ETA: sample gate, 999-minute boundary, long books, completion, rolling window and 64-bit limits PASS");
}
