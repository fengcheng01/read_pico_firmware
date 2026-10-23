/* SPDX-License-Identifier: Apache-2.0
 * 中文：真实本地49相派生序列、每种动作剂量与PSRAM失败回退回归。
 * English: Real local49-phase derived sequence, per-action doses and PSRAM failure fallback regression.
 */
#include "display_animation.h"
#include "e0470_epaper_waveform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool fail;
static unsigned allocations;
void* heap_caps_malloc(size_t bytes,unsigned caps) {assert(caps==3&&bytes==E0470_GL16_FRAMES*64u);++allocations;return fail?NULL:malloc(bytes);}
int main(void) {
    e0470_waveform_init();
    const EpdWaveformPhases* base=e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM,MODE_GL16);
    assert(base&&base->phases==49);
    uint8_t original[49*64], derived[49*64];memcpy(original,base->luts,sizeof(original));
    const EpdWaveform* wave=display_animation_waveform();assert(wave!=&E0470_TEXTTURN_WAVEFORM);
    const EpdWaveformPhases* soft=e0470_waveform_phases(wave,MODE_GL16);
    assert(soft&&soft->phases==49&&!soft->phase_times&&wave->temp_intervals==E0470_TEXTTURN_WAVEFORM.temp_intervals);
    assert(display_animation_waveform()==wave&&allocations==1);
    unsigned changed=0,lower_peak=0;
    for(unsigned to=0;to<16;++to)for(unsigned from=0;from<16;++from){
        unsigned before[3]={0},after[3]={0},differences=0,first=49,last=0;
        int original_net=0,soft_net=0,original_peak=0,soft_peak=0;
        for(unsigned p=0;p<49;++p){
            int a=e0470_phase_action(base,p,to,from),b=e0470_phase_action(soft,p,to,from);
            assert(a<3&&b<3);++before[a];++after[b];
            if(a){if(first==49)first=p;last=p;}
            assert((a==0)==(b==0));
            if(from!=15||to==0||to==15||to==from)assert(a==b);
            differences+=a!=b;
            original_net+=(a==1)-(a==2);soft_net+=(b==1)-(b==2);
            assert(soft_net<=original_net);
            if(original_net>original_peak)original_peak=original_net;
            if(soft_net>soft_peak)soft_peak=soft_net;
        }
        assert(!memcmp(before,after,sizeof(before))&&differences<=4);
        if(first<49){assert(e0470_phase_action(base,first,to,from)==e0470_phase_action(soft,first,to,from));assert(e0470_phase_action(base,last,to,from)==e0470_phase_action(soft,last,to,from));}
        changed+=differences;
        assert(original_net==soft_net&&soft_peak<=original_peak);
        lower_peak+=soft_peak<original_peak;
    }
    assert(changed>0&&lower_peak>0&&!memcmp(original,base->luts,sizeof(original)));
    memcpy(derived,soft->luts,sizeof(derived));display_animation_release();display_animation_release();
    memset(derived,0xA5,sizeof(derived));
    assert(!display_animation_build(derived,sizeof(derived)-1,original,49)&&derived[0]==0xA5);
    assert(!display_animation_build(derived,sizeof(derived),NULL,49));
    assert(!display_animation_build(derived,sizeof(derived),original,0));
    assert(!display_animation_build(derived,sizeof(derived),original,65));
    assert(!display_animation_build(original,sizeof(original),original,49));
    assert(!display_animation_build(original+1,sizeof(original),original,49));
    uint8_t invalid[49*64];memcpy(invalid,original,sizeof(invalid));invalid[50]|=3;
    assert(!display_animation_build(derived,sizeof(derived),invalid,49)&&derived[0]==0xA5);
    for(unsigned i=0;i<200;++i){
        fail=i%2==0;wave=display_animation_waveform();assert((wave==&E0470_TEXTTURN_WAVEFORM)==fail);
        display_animation_release();
    }
    assert(!memcmp(original,base->luts,sizeof(original)));
    printf("Animation:49 phases/256 pairs, %u changed actions; preserved doses/holds/endpoints, untouched other transitions, immutable source, overlap/invalid/OOM and200 release/retry cycles PASS\n",changed);
}
