#include "highscores.hpp"
#include "hardware.hpp"
#include <cstdio>
#include <cstring>

namespace highscores {
    static Entry g_entries[NUM_SCORES];
    static bool g_loaded = false;

    static void set_default() {
        for(int i=0;i<NUM_SCORES;i++) {
            g_entries[i].score = (NUM_SCORES - i) * 500u;
            g_entries[i].level = 1;
            std::snprintf(g_entries[i].name, MAX_NAME+1, "PLAYER %d", i+1);
        }
    }

    void init() {
        if(g_loaded) return;
        FILE* f = fopen("sdmc:/ballistica/scores.dat", "rb");
        if(!f) {
            set_default();
            g_loaded = true;
            save();
            hw_log("scores default\n");
            return;
        }
        bool ok = true;
        for(int i=0;i<NUM_SCORES;i++) {
            uint32_t sc; uint8_t lvl; char line[128];
            if(fread(&sc, sizeof(sc), 1, f)!=1) { ok=false; break; }
            if(fread(&lvl, sizeof(lvl), 1, f)!=1) { ok=false; break; }
            int cpos=0; int ch; while(cpos < (int)sizeof(line)-1 && (ch=fgetc(f))!=EOF && ch!='\n') line[cpos++]=(char)ch; line[cpos]='\0';
            if(ch==EOF && i<NUM_SCORES-1) { ok=false; break; }
            g_entries[i].score=sc; g_entries[i].level=lvl; {
                int k=0; for(; k<MAX_NAME && line[k]; ++k) g_entries[i].name[k]=line[k];
                g_entries[i].name[k]='\0';
            }
        }
        fclose(f);
        if(!ok) { set_default(); hw_log("scores corrupt reset\n"); }
        g_loaded = true;
    }

    int submit(uint32_t score, int level) {
        init();
        int pos = -1;
        for(int i=0;i<NUM_SCORES;i++) { if(score > g_entries[i].score) { pos = i; break; } }
        if(pos < 0) return -1;
        // shift down
        for(int j=NUM_SCORES-2; j>=pos; --j) g_entries[j+1] = g_entries[j];
        g_entries[pos].score = score;
        g_entries[pos].level = (level<0?0:(level>255?255:level));
        g_entries[pos].name[0] = '\0';
        return pos;
    }

    void set_name(int index, const char* name) {
        if(index<0 || index>=NUM_SCORES) return;
        if(!name) name = "PLAYER";
        std::strncpy(g_entries[index].name, name, MAX_NAME);
        g_entries[index].name[MAX_NAME]='\0';
    }

    void save() {
        FILE* f = fopen("sdmc:/ballistica/scores.dat", "wb");
        if(!f) { hw_log("save scores fail\n"); return; }
        for(int i=0;i<NUM_SCORES;i++) {
            fwrite(&g_entries[i].score, sizeof(uint32_t), 1, f);
            fwrite(&g_entries[i].level, sizeof(uint8_t), 1, f);
            fputs(g_entries[i].name, f); fputc('\n', f);
        }
        fclose(f);
    }

    const Entry* table() { init(); return g_entries; }
}
