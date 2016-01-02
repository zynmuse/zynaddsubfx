/*
  ZynAddSubFX - a software synthesizer

  Master.h - It sends Midi Messages to Parts, receives samples from parts,
             process them with system/insertion effects and mix them
  Copyright (C) 2002-2005 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2 or later) for more details.

  You should have received a copy of the GNU General Public License (version 2)
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

*/

#ifndef MASTER_H
#define MASTER_H
#include "../globals.h"
#include "Microtonal.h"
#include <rtosc/miditable.h>
#include <rtosc/ports.h>

#include "Time.h"
#include "Bank.h"
#include "Recorder.h"

#include "../Params/Controller.h"

class Allocator;

struct vuData {
    vuData(void);
    float outpeakl, outpeakr, maxoutpeakl, maxoutpeakr,
          rmspeakl, rmspeakr;
    int clipped;
};


/** It sends Midi Messages to Parts, receives samples from parts,
 *  process them with system/insertion effects and mix them */
class Master
{
    public:
        /** Constructor TODO make private*/
        Master(const SYNTH_T &synth, class Config *config);
        /** Destructor*/
        ~Master();

        char last_xmz[XMZ_PATH_MAX];

        void applyOscEvent(const char *event);

        /**Saves all settings to a XML file
         * @return 0 for ok or <0 if there is an error*/
        int saveXML(const char *filename);

        /**This adds the parameters to the XML data*/
        void add2XML(XMLwrapper& xml);

        void defaults();

        /**loads all settings from a XML file
         * @return 0 for ok or -1 if there is an error*/
        int loadXML(const char *filename);

        /**Regenerate PADsynth and other non-RT parameters
         * It is NOT SAFE to call this from a RT context*/
        void applyparameters(void) NONREALTIME;

        //This must be called prior-to/at-the-time-of RT insertion
        void initialize_rt(void) REALTIME;

        void getfromXML(XMLwrapper& xml);

        /**get all data to a newly allocated array (used for plugin)
         * @return the datasize*/
        int getalldata(char **data) NONREALTIME;
        /**put all data from the *data array to zynaddsubfx parameters (used for plugin)*/
        void putalldata(const char *data);

        //Midi IN
        void noteOn(char chan, char note, char velocity);
        void noteOff(char chan, char note);
        void polyphonicAftertouch(char chan, char note, char velocity);
        void setController(char chan, int type, int par);
        //void NRPN...


        void ShutUp();
        int shutup;

        void vuUpdate(const float *outl, const float *outr);

        /**Audio Output*/
        void AudioOut(float *outl, float *outr) REALTIME;
        /**Audio Output (for callback mode).
         * This allows the program to be controled by an external program*/
        void GetAudioOutSamples(size_t nsamples,
                                unsigned samplerate,
                                float *outl,
                                float *outr) REALTIME;


        void partonoff(int npart, int what);

        //Set callback to run when master changes
        void setMasterChangedCallback(void(*cb)(void*,Master*),void *ptr);

        /**parts \todo see if this can be made to be dynamic*/
        class Part * part[NUM_MIDI_PARTS];

        //parameters

        unsigned char Pvolume;
        unsigned char Pkeyshift;
        unsigned char Psysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        unsigned char Psysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        //parameters control
        void setPvolume(char Pvolume_);
        void setPkeyshift(char Pkeyshift_);
        void setPsysefxvol(int Ppart, int Pefx, char Pvol);
        void setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol);

        //effects
        class EffectMgr * sysefx[NUM_SYS_EFX]; //system
        class EffectMgr * insefx[NUM_INS_EFX]; //insertion
//      void swapcopyeffects(int what,int type,int neff1,int neff2);

        //HDD recorder
        Recorder HDDRecorder;

        //part that's apply the insertion effect; -1 to disable
        short int Pinsparts[NUM_INS_EFX];


        //peaks for VU-meter
        void vuresetpeaks();

        //peaks for part VU-meters
        float vuoutpeakpart[NUM_MIDI_PARTS];
        unsigned char fakepeakpart[NUM_MIDI_PARTS]; //this is used to compute the "peak" when the part is disabled

        AbsTime  time;
        Controller ctl;
        bool       swaplr; //if L and R are swapped

        //other objects
        Microtonal microtonal;

        //Strictly Non-RT instrument bank object
        Bank bank;

        class FFTwrapper * fft;

        static const rtosc::Ports &ports;
        float  volume;

        //Statistics on output levels
        vuData vu;

        rtosc::MidiMapperRT midi;

        bool   frozenState;//read-only parameters for threadsafe actions
        Allocator *memory;
        rtosc::ThreadLink *bToU;
        rtosc::ThreadLink *uToB;
        bool pendingMemory;
        
        off_t  offl = 0, offr = 0;
        
        template<class StereoType>
        void GetAudioOutSamplesStereo(size_t nsamples,
                                unsigned samplerate,
                                StereoType* outs)
{
    off_t out_off = 0;
    if(outs == nullptr)
        printf("WARNING!!! !outs\n");

    //Fail when resampling rather than doing a poor job
    if(synth.samplerate != samplerate) {
        printf("darn it: %d vs %d\n", synth.samplerate, samplerate);
        return;
    }
    
    const auto zip_buffers = [&](std::size_t n_smps) {
        StereoType* i = outs + out_off;
        const StereoType* const iend = i + n_smps;
        const float *jl = bufl + off,
            *jr = bufr + off;
        for(; i < iend; ++i, ++jl, ++jr)
            (*i)[0] = *jl, (*i)[1] = *jr;
    };
    
    while(nsamples) {
        //use all available samples
        if(nsamples >= smps) {
            
            zip_buffers(smps);
            
            //memcpy(outl + out_off, bufl + off, sizeof(float) * smps);
            //memcpy(outr + out_off, bufr + off, sizeof(float) * smps);
            nsamples -= smps;

            //generate samples
            AudioOut(bufl, bufr);
            off  = 0;
            out_off  += smps;
            smps = synth.buffersize;
        }
        else {   //use some samples
        
            zip_buffers(nsamples);
        
            smps    -= nsamples;
            off     += nsamples;
            nsamples = 0;
        }
    }
}
        
        const SYNTH_T &synth;
        const int& gzip_compression; //!< value from config
    private:
        float  sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float  sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];
        int    keyshift;

        //information relevent to generating plugin audio samples
        float *bufl;
        float *bufr;
        off_t  off;
        size_t smps;

        //Callback When Master changes
        void(*mastercb)(void*,Master*);
        void* mastercb_ptr;
};

#endif
