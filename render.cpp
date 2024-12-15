#include <Bela.h>
#include <libraries/ne10/NE10.h> // NEON FFT library
#include <libraries/Midi/Midi.h>
#include <cmath>
#include <string.h>
#include <vector>
#include <complex>
#include <libraries/Trill/Trill.h>
#include <libraries/Oscillator/Oscillator.h>

//SET UP SENSOR----------------------------------------------------------------------------------------------
#define NUM_TOUCH 5 // Number of touches on Trill sensor

Trill touchSensor;
float gTouchLocation[NUM_TOUCH] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
float gTouchSize[NUM_TOUCH] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
int gNumActiveTouches = 0;
unsigned int gTaskSleepTime = 12000; // microseconds
int gSampleCount = 0; 
Oscillator osc[NUM_TOUCH];

float gAmplitudeRange[2] = { 0.0, 1.0 } ;
float gGain = 3200;

//SET UP VOICE CHANGER----------------------------------------------------------------------------------------------
#define BUFFER_SIZE 16384 //change from 16384 to 128

int gAudioChannelNum; // number of audio channels to iterate over

float gInputBuffer[BUFFER_SIZE];
int gInputBufferPointer = 0;
float gOutputBuffer[BUFFER_SIZE];
int gOutputBufferWritePointer = 0;
int gOutputBufferReadPointer = 0;
float *gWindowBuffer;

int gFFTSize = 2048; //change from 2048 to 1024
int gHopSize = 256;
int gPeriod = 256;
float gFFTScaleFactor = 0;

// FFT vars
ne10_fft_cpx_float32_t* timeDomainIn;
ne10_fft_cpx_float32_t* timeDomainOut;
ne10_fft_cpx_float32_t* frequencyDomain;
ne10_fft_cfg_float32_t cfg;

// Auxiliary task for calculating FFT
AuxiliaryTask gFFTTask;
int gFFTInputBufferPointer = 0;
int gFFTOutputBufferPointer = 0;

void process_fft_background(void*);

int gEffect = 0;
enum{
	kBypass,
	kRobot,
	kWhisper,
	kGirl,
	kDemon,
	kEcho,
	kAlien,
};

float pitchShiftFactor = 4.0f; //提高音高因子
float *gInputAudio = NULL;
Midi midi;

void loop(void*)
{
	while(!Bela_stopRequested())
	{
		// Read locations from Trill sensor
		touchSensor.readI2C();
		gNumActiveTouches = touchSensor.getNumTouches();
		for(unsigned int i = 0; i <  gNumActiveTouches; i++) {
			gTouchLocation[i] = touchSensor.touchLocation(i);
			gTouchSize[i] = touchSensor.touchSize(i);
		}
		// For all inactive touches, set location and size to 0
		for(unsigned int i = gNumActiveTouches; i < NUM_TOUCH; i++) {
			gTouchLocation[i] = 0;
			gTouchSize[i] = 0;
		}
		usleep(gTaskSleepTime);
	}
}

void midiCallback(MidiChannelMessage message, void* arg){
	bool shouldPrint = false;
	if(message.getType() == kmmControlChange){
		float data = message.getDataByte(1) / 127.0f;
		switch (message.getDataByte(0)){
		case 21 :
			gEffect = (int)(data * 6 + 0.5); // CC2 selects an effect between 0,1,2,3,4,5,6
			break;
		default:
			shouldPrint = true;
		}
	}
	if(shouldPrint){
		message.prettyPrint();
	}
}

bool setup(BelaContext* context, void* userData)
{
	// Setup a Trill Bar sensor on i2c bus 1, using the default mode and address
	if(touchSensor.setup(1, Trill::BAR) != 0) {
		fprintf(stderr, "Unable to initialise Trill Bar\n");
		return false;
	}
	touchSensor.printDetails();

	// Set and schedule auxiliary task for reading sensor data from the I2C bus
	Bela_runAuxiliaryTask(loop);

	gAudioChannelNum = std::min(context->audioInChannels, context->audioOutChannels);

	// Check that we have the same number of inputs and outputs.
	if(context->audioInChannels != context->audioOutChannels){
		printf("Different number of audio outputs and inputs available. Using %d channels.\n", gAudioChannelNum);
	}

	midi.readFrom(0);
	midi.setParserCallback(midiCallback);

	gFFTScaleFactor = 1.0f / (float)gFFTSize;
	gOutputBufferWritePointer += gHopSize;

	timeDomainIn = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	timeDomainOut = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	frequencyDomain = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	cfg = ne10_fft_alloc_c2c_float32_neon (gFFTSize);

	memset(timeDomainOut, 0, gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	memset(gOutputBuffer, 0, BUFFER_SIZE * sizeof(float));

	// Allocate buffer to mirror and modify the input
	gInputAudio = (float *)malloc(context->audioFrames * gAudioChannelNum * sizeof(float));
	if(gInputAudio == 0)
		return false;

	// Allocate the window buffer based on the FFT size
	gWindowBuffer = (float *)malloc(gFFTSize * sizeof(float));
	if(gWindowBuffer == 0)
		return false;

	// Calculate a Hann window
	for(int n = 0; n < gFFTSize; n++) {
		gWindowBuffer[n] = 0.5f * (1.0f - cosf(2.0f * M_PI * n / (float)(gFFTSize - 1)));
	}

	// Initialise auxiliary tasks
	if((gFFTTask = Bela_createAuxiliaryTask(&process_fft_background, 90, "fft-calculation")) == 0)
		return false;
	return true;
}

// This function handles the FFT processing in this example once the buffer has been assembled.
void process_fft(float *inBuffer, int inWritePointer, float *outBuffer, int outWritePointer)
{
	std::vector<std::complex<float>> tempFrequencyDomain(gFFTSize, std::complex<float>(0, 0)); //声明临时变量
	// Copy buffer into FFT input
	int pointer = (inWritePointer - gFFTSize + BUFFER_SIZE) % BUFFER_SIZE;
	for(int n = 0; n < gFFTSize; n++) {
		timeDomainIn[n].r = (ne10_float32_t) inBuffer[pointer] * gWindowBuffer[n];
		timeDomainIn[n].i = 0;

		pointer++;
		if(pointer >= BUFFER_SIZE)
			pointer = 0;
	}

	// Run the FFT
	ne10_fft_c2c_1d_float32_neon (frequencyDomain, timeDomainIn, cfg, 0);
	//Define different effects 
	switch (gEffect){
		case kBypass:
			// Bypass = 0
			break;
		
		case kRobot:
			// Robotise the output = 1
			for(int n = 0; n < gFFTSize; n++) {
				float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
				float phase = atan2f(frequencyDomain[n].i, frequencyDomain[n].r);
				frequencyDomain[n].r = amplitude * cosf(phase);
				frequencyDomain[n].i = amplitude * sinf(phase); //保留一部分原始相位的信息
			}
			break;
		
		case kWhisper:
		{
			// Whisperize the output = 2
			float gain = 20.0;
			for(int n = 0; n < gFFTSize; n++) {
				float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
				float phase = rand()/(float)RAND_MAX * 2.f* M_PI;
				float window = 0.5 * (1 - cos(2 * M_PI * n / (gFFTSize - 1))); // Hanning窗，增加平滑相位, 但是gain变小了很多
				frequencyDomain[n].r = gain * window * cosf(phase) * amplitude;
				frequencyDomain[n].i = gain * window * sinf(phase) * amplitude;
			}
			break;
		}
			
		case kGirl:
		{
    		std::vector<std::complex<float>> tempFrequencyDomain(gFFTSize, std::complex<float>(0, 0));
		    for(int n = 0; n < gFFTSize / 2; n++) {
		        int shiftedIndex = static_cast<int>(n * pitchShiftFactor); 
		        if (shiftedIndex < gFFTSize / 2) {
		            float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
		            float phase = atan2f(frequencyDomain[n].i, frequencyDomain[n].r);
		            tempFrequencyDomain[shiftedIndex].real(cosf(phase) * amplitude);
		            tempFrequencyDomain[shiftedIndex].imag(sinf(phase) * amplitude);
		        }
		    }
		    // 填补空隙，避免失真
		    for(int n = 1; n < gFFTSize / 2; n++) {
		        if (tempFrequencyDomain[n] == std::complex<float>(0, 0)) {
		            tempFrequencyDomain[n] = (tempFrequencyDomain[n-1] + tempFrequencyDomain[n+1]) / 2.0f;
		        }
		    }
		    // 将结果复制回原始数组
		    for(int n = 0; n < gFFTSize; n++) {
		        frequencyDomain[n].r = tempFrequencyDomain[n].real();
		        frequencyDomain[n].i = tempFrequencyDomain[n].imag();
		    }
			break;
		}
	
		case kDemon:
		{
	        // Demon = 4
	        for (int n = 0; n < gFFTSize / 2; n++) {
	            int shiftedIndex = static_cast<int>(n / pitchShiftFactor); // Lower pitch
	            if (shiftedIndex < gFFTSize / 2) {
	                float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
	                float phase = atan2f(frequencyDomain[n].i, frequencyDomain[n].r);
	                frequencyDomain[shiftedIndex].r = cosf(phase) * amplitude;
	                frequencyDomain[shiftedIndex].i = sinf(phase) * amplitude;
	            }
        	}
        	break;
    	}
		    
		case kEcho:
	    {
	        // Echo = 5
	        for (int n = 0; n < gFFTSize; n++) {
	            float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
	            float phase = atan2f(frequencyDomain[n].i, frequencyDomain[n].r);
	            frequencyDomain[n].r += cosf(phase) * amplitude * 0.5f; // Adding echo
	            frequencyDomain[n].i += sinf(phase) * amplitude * 0.5f; // Adding echo
	        }
	        break;
	    }
			    
		case kAlien:
	    {
	        // Alien = 6
	        for (int n = 0; n < gFFTSize; n++) {
	            float amplitude = sqrtf(frequencyDomain[n].r * frequencyDomain[n].r + frequencyDomain[n].i * frequencyDomain[n].i);
	            float phase = atan2f(frequencyDomain[n].i, frequencyDomain[n].r) + (float)n / gFFTSize * 2.0f * M_PI;
	            frequencyDomain[n].r = cosf(phase) * amplitude;
	            frequencyDomain[n].i = sinf(phase) * amplitude;
	        }
	        break;
	     break;
	    }
	
	}

	// Run the inverse FFT
	ne10_fft_c2c_1d_float32_neon (timeDomainOut, frequencyDomain, cfg, 1);
	// Overlap-and-add timeDomainOut into the output buffer
	pointer = outWritePointer;
	for(int n = 0; n < gFFTSize; n++) {
		outBuffer[pointer] += (timeDomainOut[n].r) * gFFTScaleFactor;
		if(std::isnan(outBuffer[pointer]))
			rt_printf("outBuffer OLA\n");
		pointer++;
		if(pointer >= BUFFER_SIZE)
			pointer = 0;
	}
}

// Function to process the FFT in a thread at lower priority
void process_fft_background(void*) {
	process_fft(gInputBuffer, gFFTInputBufferPointer, gOutputBuffer, gFFTOutputBufferPointer);
}

void render(BelaContext* context, void* userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
        // Map the first touch location to gDryWet and the average touch size to gGain
        if(gNumActiveTouches > 0) {
            float totalSize = 0;
            for(unsigned int i = 0; i < gNumActiveTouches; i++) {
                totalSize += gTouchSize[i];
            }
            gGain = map(totalSize / gNumActiveTouches, 0, 1, 50, 800); // Assuming gTouchSize ranges from 0 to 1
        }
        
        // Combine channels and store in input buffer
        int numAudioFrames = context->audioFrames;
        for(int n = 0; n < numAudioFrames; n++) {
            // 从音频输入设备读取数据
            gInputBuffer[gInputBufferPointer] = ((audioRead(context, n, 0) + audioRead(context, n, 1)) * 0.5);
    
            // Copy output buffer to output
            for(int channel = 0; channel < gAudioChannelNum; channel++){
                audioWrite(context, n, channel, gOutputBuffer[gOutputBufferReadPointer] * gGain);
            }
    
            // Clear the output sample in the buffer so it is ready for the next overlap-add
            gOutputBuffer[gOutputBufferReadPointer] = 0;
            gOutputBufferReadPointer++;
            if(gOutputBufferReadPointer >= BUFFER_SIZE)
                gOutputBufferReadPointer = 0;
            gOutputBufferWritePointer++;
            if(gOutputBufferWritePointer >= BUFFER_SIZE)
                gOutputBufferWritePointer = 0;
    
            gInputBufferPointer++;
            if(gInputBufferPointer >= BUFFER_SIZE)
                gInputBufferPointer = 0;
            
            gSampleCount++;
            if(gSampleCount >= gHopSize) {
                gFFTInputBufferPointer = gInputBufferPointer;
                gFFTOutputBufferPointer = gOutputBufferWritePointer;
                Bela_scheduleAuxiliaryTask(gFFTTask);
                gSampleCount = 0;
            }
		}
        gHopSize = gPeriod;
    }
}

void cleanup(BelaContext* context, void* userData)
{
	NE10_FREE(timeDomainIn);
	NE10_FREE(timeDomainOut);
	NE10_FREE(frequencyDomain);
	NE10_FREE(cfg);
	free(gInputAudio);
	free(gWindowBuffer);
}
