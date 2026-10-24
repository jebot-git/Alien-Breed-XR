/* Focused regression check; run with tools/check_feedback.py. */
#include "../src/audio.c"
#include <assert.h>
#undef printf
static double tone(int rate, int hz, int submerged) {
    g_spec.freq=rate; audio_filter_init(); g_underwater=submerged;
    double energy=0; int n=rate/2;
    for(int i=0;i<n;i++) {
        Sint16 sample=(Sint16)(12000*sin(6.283185307179586*hz*i/rate));
        Sint16 dry=sample;
        audio_filter_output((Uint8 *)&sample,sizeof(sample));
        if(!submerged) assert(sample==dry);
        if(i>rate/10) energy+=(double)sample*sample;
    }
    return sqrt(energy/(n-rate/10-1));
}
int main(void) {
    int rates[]={8007,48000};
    for(int r=0;r<2;r++) {
        int rate=rates[r];
        double low=tone(rate,300,1)/tone(rate,300,0);
        double high=tone(rate,rate==8007?3800:10000,1)/tone(rate,rate==8007?3800:10000,0);
        assert(low>0.98 && low<1.02);
        assert(high<0.15);
        printf("PASS rate=%d: low gain %.3f high gain %.3f; dry bypass exact\n",rate,low,high);
        g_spec.freq=rate; audio_filter_init(); g_underwater=1;
        Sint16 samples[2048]={0};
        audio_filter_output((Uint8*)samples,sizeof(samples));
        assert(g_filter_wet==1.0f);
        g_underwater=0;
        for(int j=0;j<2;j++) audio_filter_output((Uint8*)samples,sizeof(samples));
        assert(g_filter_wet==0.0f);
        for(int j=0;j<2048;j++) assert(samples[j]==0);
    }
}
