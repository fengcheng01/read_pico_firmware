/* SPDX-License-Identifier: Apache-2.0
 * 中文：链接真实28相表与错相引擎，检查每像素完整序列、旋转、提交、OOM及逐拍失败。
 * English: Link the actual28-phase table and engine to verify per-pixel phases, rotations, commit, OOM and failure at every tick.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "e0470_page_turn.h"
#include "e0470_epaper_waveform.h"
#include "esp_heap_caps.h"

#define W 144
#define H 70
#define BYTES (W*H/2)
static uint8_t front[BYTES],back[BYTES],old[BYTES],diff[W*H],cols[W/2];
static bool lines[H],oom;
static unsigned seen[W*H],live,scans,differences,padding,cleared;
static int fail_tick=-1;
static EpdRotation rotation;
static const uint8_t*const* phase_luts;
static const int8_t* line_phases;
static const int *band0,*band1;
static const int8_t* band_phases;
static int band_count;
int epd_width(void){return W;}
int epd_height(void){return H;}
EpdRotation epd_get_rotation(void){return rotation;}
EpdRect epd_full_screen(void){return(EpdRect){0,0,W,H};}
void epd_poweron(void){}
int64_t esp_timer_get_time(void){static int64_t t;return t+=100;}
void esp_rom_delay_us(uint32_t us){assert(us);++padding;}
void* heap_caps_aligned_alloc(size_t alignment,size_t n,unsigned caps){
    assert(alignment==16&&n==64*1024&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(oom)return NULL;
    void*p=malloc(n);assert(p);++live;return p;
}
void heap_caps_free(void*p){if(p){assert(live);--live;free(p);}}
void epd_build_1ppB_lut_1k(uint8_t* lut,const EpdWaveformPhases* phases,int p){
    memset(lut,0,1024);
    for(int pair=0;pair<256;++pair)lut[pair]=(uint8_t)e0470_phase_action(phases,p,pair>>4,pair&15);
    lut[768]=(uint8_t)p;
}
void epd_set_line_phase_luts(const uint8_t*const*luts,const int8_t*phases){
    assert(!phase_luts);phase_luts=luts;line_phases=phases;band_count=0;
}
void epd_set_col_phase_luts(const uint8_t*const*luts,const int*a,const int*b,const int8_t*p,int n){
    assert(!phase_luts);phase_luts=luts;line_phases=NULL;band0=a;band1=b;band_phases=p;band_count=n;
}
void epd_clear_phase_luts(void){assert(phase_luts);phase_luts=NULL;line_phases=NULL;++cleared;}
static unsigned nibble(const uint8_t*fb,unsigned pixel){return(fb[pixel/2]>>((pixel&1)*4))&15;}
void epd_difference_image_cropped(const uint8_t*a,const uint8_t*b,EpdRect r,uint8_t*out,bool*dl,uint8_t*dc){
    assert(r.x==0&&r.y==0&&r.width==W&&r.height==H);++differences;
    memset(dl,0,H);memset(dc,0,W/2);
    for(unsigned i=0;i<W*H;++i)out[i]=(uint8_t)(nibble(a,i)*16+nibble(b,i));
}
enum EpdDrawError epd_draw_base(EpdRect r,const uint8_t* data,EpdRect crop,enum EpdDrawMode mode,int temp,
                              const bool*dl,const uint8_t*dc,const EpdWaveform*wave){
    assert(phase_luts&&data==diff&&r.width==W&&crop.height==H&&temp==25);
    assert(mode==(MODE_PACKING_1PPB_DIFFERENCE|MODE_DU)&&wave==&E0470_APPLY_WAVEFORM);
    assert(!memcmp(back,old,BYTES));
    if((int)scans++==fail_tick)return EPD_DRAW_OTHER_ERROR;
    const EpdWaveformPhases*src=e0470_waveform_phases(&E0470_GRAY_BALANCED_WAVEFORM,MODE_GL16);
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){
        int p=line_phases?line_phases[y]:-1;
        if(!line_phases)for(int b=0;b<band_count;++b)if(x>=band0[b]&&x<band1[b])p=band_phases[b];
        if(p<0)continue;
        assert(dl[y]&&dc[x/2]&&p<28);
        unsigned i=(unsigned)y*W+x;
        assert((int)seen[i]==p);++seen[i];
        assert(phase_luts[p][768]==p);
        assert(phase_luts[p][data[i]]==e0470_phase_action(src,p,data[i]>>4,data[i]&15));
    }
    return EPD_DRAW_SUCCESS;
}
static void reset(void){
    assert(!live&&!phase_luts);
    memset(seen,0,sizeof(seen));scans=differences=padding=cleared=0;
    for(unsigned i=0;i<BYTES;++i){front[i]=(uint8_t)(i*37+19);back[i]=(uint8_t)(i*13+7);}
    memcpy(old,back,BYTES);
}
int main(void){
    e0470_waveform_init();
    EpdiyHighlevelState hl={front,back,diff,lines,cols};
    e0470_page_turn_set_tick_us(0);
    for(int rot=0;rot<4;++rot)for(int dir=0;dir<4;++dir){
        reset();rotation=(EpdRotation)rot;fail_tick=-1;
        EpdRect area={0,0,rot%2?H:W,rot%2?W:H};
        assert(e0470_page_turn_with_waveform_compact(&hl,area,(e0470_turn_dir_t)dir,&E0470_GRAY_BALANCED_WAVEFORM,8)==EPD_DRAW_SUCCESS);
        assert(scans==35&&differences==1&&cleared==35&&!padding);
        for(unsigned i=0;i<W*H;++i)assert(seen[i]==28);
        assert(!memcmp(front,back,BYTES));e0470_page_turn_release();assert(!live);
    }
    rotation=EPD_ROT_PORTRAIT;
    for(int tick=0;tick<35;++tick){
        reset();fail_tick=tick;
        assert(e0470_page_turn_with_waveform_compact(&hl,(EpdRect){0,0,H,W},E0470_TURN_RTL,&E0470_GRAY_BALANCED_WAVEFORM,8)==EPD_DRAW_OTHER_ERROR);
        assert(scans==(unsigned)tick+1&&cleared==scans&&!memcmp(back,old,BYTES));e0470_page_turn_release();
    }
    fail_tick=-1;reset();oom=true;
    assert(e0470_page_turn_with_waveform_compact(&hl,(EpdRect){0,0,H,W},E0470_TURN_RTL,&E0470_GRAY_BALANCED_WAVEFORM,8)==EPD_DRAW_NO_PHASES_AVAILABLE);
    assert(!live&&!scans&&!differences&&!memcmp(back,old,BYTES));oom=false;
    for(int i=0;i<100;++i){
        reset();assert(e0470_page_turn_with_waveform_compact(&hl,(EpdRect){0,0,H,W},E0470_TURN_RTL,&E0470_GRAY_BALANCED_WAVEFORM,8)==EPD_DRAW_SUCCESS);e0470_page_turn_release();
    }
    puts("Real28-phase engine:4 rotations/4 directions, all pixels incl footer retain28 ordered phases,35 ticks/one diff/no padding,35 failure points/no commits,OOM and100 release cycles PASS");
}
