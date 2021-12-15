/*
  ZynAddSubFX - a software synthesizer

  Granular.h - Granulareration effect
  Copyright (C) 2002-2009 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/

#ifndef Granular_H
#define Granular_H

#include "Effect.h"

#define MAXGRAINS 16
#define NUMSAMPLES 8
#define FADEIN_COUNT 20 // will fade the first and last samples of a repeat 

namespace zyn {
struct Sample{
  float *left;
  float *right;
  int numbuffers;
  bool inuse;
  int repeatsnum; // how many times will we repeat this sample -1 =forever
  int repeatscounter;
  int numActiveRepeats;
  int triggertimer;
  int repeatstriggertime; // How often to lauch a repeat
  int loadcounter; // Used to count up buffers when loading data into sample
  float max; // The maximum level within the sample 
};
struct Grain {
  int timer;
  int inuse;
  int samplenumber;
  Sample *sample;
  int length;
  bool reverse;
  float fade;
  int repeatnumber;
  float repeatcross; // 0-1 the degree to which this repeat is positioned l/r
  int fadeinout; // used to eliminate clicks at start and endof a repeat by counting down
};

/**Creates Granulareration Effects*/
class Granular:public Effect
{
    public:
        Granular(EffectParams pars);
        ~Granular();
        void out(const Stereo<float *> &smp);
        void cleanup(void);

        unsigned char getpresetpar(unsigned char npreset, unsigned int npar);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;

        static rtosc::Ports ports;
    private:
        //Parametrii
        unsigned char Pvolume;
        unsigned char Pprogram;        //duration
        unsigned char Ptriglevel;   // Pidelay;      //initial delay
        unsigned char Plength;        // Pidelayfb;    //initial feedback
        unsigned char Prepeatstime;        //Plpf;
        unsigned char Prepeatsnum;        //Phpf;
        unsigned char Prepeatslen;        //Plohidamp;    //Low/HighFrequency Damping
        unsigned char Pfwdreverse;        //Bitmask bits 0-1 0=fwd, 1=alternate, 2=reverse, 3=Remote control, Bit 2=Remote Control.
        unsigned char Pfade;        //Proomsize;    //room size
        unsigned char Pcrossover;        //Pbandwidth;   //bandwidth
        unsigned char P11;        //
        unsigned char P12;        //
                // The following parameters are not intended to appear on display but are used by external automation
        // The value placed in the following parameters will be the sample number which will be cleared by the effect when processed
        unsigned char Pforcesample; // used for external control to fill a sample with immediate audio 
        bool initialized;
        float ms1, ms2, ms3, ms4; //mean squares
        float ampsns, ampsmooth;

        uint32_t ctr; //A general purpose loop counter for debugging
        uint32_t slowctr; // Slower counter used for debugging
        float rms_max;
        float rms_min;
        float rms_mid;
        float triglevel;
        int repeatslen;  // number of buffers in the grain from the repeatslength parameter
        int samplelen;  // number of buffers in the sample from the sample length parameter
        int repeatstriggertime, repeatsnum;
        int loadcycle; 
        int nextgrain;
        int nextsample;
        Sample sample[NUMSAMPLES];
        Sample *currsample;
        Grain grain[MAXGRAINS];
        bool revctr;
        float repeatcross; //How far left and right for repeat crossover
        bool crossctr; //Alternative repeats may be sent to left and then right channel
        int mode; // top 6 bits from Pfwdreverse. O indicated local mode sampling, Otherwise sample and play by remote triggers
        int forcesample;
        uint32_t statusbits; // from zyn to display need to poll for this data
        uint32_t repeatbits; // from zyn to display need to poll for this data
        uint32_t requestbits; // from display to zyn control samples and repeats
        int prevSample; // The sample that was last launched
        int TotalGrains; // for debugging - how many grains are currently in use
        float saveefxoutl; // for debugging
        float saveefxoutr; // for debugging
        //parameter control
        void setvolume(unsigned char _Pvolume);
        void setprogram(unsigned char _Pprogram);
        void setrepeatslen(unsigned char _Prepeatslen);
        void settriglevel(unsigned char _Ptriglevel);
        void setlength(unsigned char _Plength);
        void setrepeatsnum(unsigned char _Prepeatsnum);
        void setrepeatstime(unsigned char _Prepeatstime);
        void setfwdreverse(unsigned char _Pfdwreverse);
        void setfade(unsigned char _Pfade);
        void setcrossover(unsigned char _Pcrossover);
        void setp11(unsigned char _P11);
        void setp12(unsigned char _P12);

  float getfadefactor(int repeat, int total);
  bool getdirection();
  int getmode();
  void remoteMode(const Stereo<float *> &input);
  void localMode(const Stereo<float *> &input);

  void playgrain(Grain *cgrain, Sample *gsample);
  void playgrains(void);
  void TerminateGrain(int n);
  void TerminateGrains(int samplenumber);
  void launchrepeat(Sample *sample, int samplenumber);
  void launchSampleRepeatsWhenRequired(Sample *sample, int samplenumber);
  int setrepeatstime(Sample *csample);
  void launchrepeatsWhenRequired(void);
  void startSample(Sample *currsample, int samplenumber);
  bool continueSample(Sample *currsample, const Stereo<float *> &input);
  int TimeToBuffers(float t) {
    return (int)(float)(t*samplerate_f/buffersize_f);
  }


        //Parameters

        //Internal Variables
};

}

#endif
