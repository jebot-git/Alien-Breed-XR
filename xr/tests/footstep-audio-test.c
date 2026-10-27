#include "../../src/audio.c"
#include <assert.h>
#undef printf
static const char *directory;
void io_make_data_path(char *out,size_t size,const char *path){snprintf(out,size,"%s/%s",directory,path);}
int ab3d_log_printf(const char *fmt,...){(void)fmt;return 0;}
int main(int argc,char **argv){
    assert(argc==2);directory=argv[1];
    for(int rate_index=0;rate_index<2;++rate_index){
        memset(g_samples,0,sizeof(g_samples));memset(g_channels,0,sizeof(g_channels));
        g_spec.freq=rate_index?48000:8007;g_spec.format=AUDIO_S16SYS;g_spec.channels=1;
        audio_filter_init();g_master_volume=100;
        for(int id=23;id<=25;++id){
            assert(load_one_sample(id));
            double duration=g_samples[id].length/2.0/g_spec.freq;
            double expected=amiga_sfx_sizes[id]/8007.0;
            assert(fabs(duration-expected)<.002);
            audio_queue_sample(id,255);Channel *channel=NULL;
            for(int c=0;c<MAX_CHANNELS;++c)if(g_channels[c].sample_id==id)channel=&g_channels[c];
            assert(channel);Uint32 consumed=0;Uint8 chunk[256];
            while(channel->sample_data){Uint32 before=channel->position;audio_callback(NULL,chunk,sizeof(chunk));consumed+=channel->position-before;}
            assert(consumed==g_samples[id].length);
            SDL_free(g_samples[id].data);
        }
    }
    puts("PASS original footstep sample durations at 8007 and 48000 Hz; mixer consumes at normal playback speed");
}
