#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisysp;
using namespace daisy;

static DaisySeed  seed;
static Oscillator osc[4];
static MoogLadder filter[4];

static AdcChannelConfig adc_jay[12];
static GateIn NotGateIn[4];

static void AudioCallback(float *in, float *out, size_t size)
{
    float oscOut[4] = {0, 0, 0, 0};
	float sig = 0;
    for(size_t i = 0; i < size; i += 2)
    { 
		for ( uint8_t a = 0; a < 4; a++ ) {
			osc[a].SetFreq(13.75f * pow(2.f, seed.adc.GetFloat(a) * 10.667f));
			osc[a].SetAmp(0.25f * !NotGateIn[a].State() * seed.adc.GetFloat(a+4));
			oscOut[a] = osc[a].Process();
			filter[a].SetFreq(110.f * pow(2.f, seed.adc.GetFloat(a+8) * 10.667f * 0.5f));
			sig += filter[a].Process(oscOut[a]);
		}

        // left out
        out[i] = sig;

        // right out
        out[i + 1] = sig;
    }
}

int main(void)
{
    // initialize seed hardware and oscillator daisysp module
    float sample_rate;
    seed.Configure();
    seed.Init();
    sample_rate = seed.AudioSampleRate();
	
	for ( uint8_t a = 0; a < 4; a++ ) {
		osc[a].Init(sample_rate);
		osc[a].SetWaveform(osc[a].WAVE_SAW);
		filter[a].Init(sample_rate);
		filter[a].SetRes(0.5);
		dsy_gpio_pin pin = seed.GetPin(14-a);
		NotGateIn[a].Init(&pin);
	}
	
	adc_jay[0].InitSingle(seed.GetPin(15));
	adc_jay[1].InitSingle(seed.GetPin(16));
	adc_jay[2].InitSingle(seed.GetPin(17));
	adc_jay[3].InitSingle(seed.GetPin(18));
	adc_jay[4].InitSingle(seed.GetPin(19));
	adc_jay[5].InitSingle(seed.GetPin(20));
	adc_jay[6].InitSingle(seed.GetPin(21));
	adc_jay[7].InitSingle(seed.GetPin(22));
	adc_jay[8].InitSingle(seed.GetPin(23));
	adc_jay[9].InitSingle(seed.GetPin(24));
	adc_jay[10].InitSingle(seed.GetPin(25));
	adc_jay[11].InitSingle(seed.GetPin(28));
	seed.adc.Init(&adc_jay[0], 12);
	seed.adc.Start();

    // start callback
	size_t audioBsize = 1;
    seed.SetAudioBlockSize(audioBsize);
    seed.StartAudio(AudioCallback);

    while(1) {}
}
