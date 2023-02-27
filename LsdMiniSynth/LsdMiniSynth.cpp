#include <iostream>
#include <olcNoiseMaker.h>
#include <algorithm>
#include <fstream>
#include <string>


constexpr int SOURCE_LFO1 = 0;
constexpr int SOURCE_LFO2 = 1;
constexpr int SOURCE_ENV1 = 2;
constexpr int SOURCE_ENV2 = 3;
constexpr int SOURCE_OSC1 = 4;
constexpr int SOURCE_OSC2 = 5;

constexpr int OSC_SIN		= 0;
constexpr int OSC_SQR_ANA	= 1;
constexpr int OSC_SQR_DIG	= 2;
constexpr int OSC_TRI		= 3;
constexpr int OSC_SAW_ANA	= 4;
constexpr int OSC_SAW_DIG	= 5;
constexpr int OSC_NOISE		= 6;



double w(double hertz) {
	return hertz * 2 * 3.14159;
}

class Osc {
public:
	int waveform;

	Osc() {
		waveform = OSC_SIN;
	}

	Osc(int _waveform) {
		waveform = _waveform;
	}

	double getAmp(double time, double frequency, double phase = 0) {
		double output;
		switch (waveform) {
			default:
				return 0;

			case OSC_SIN: //sin
				return sin(w(frequency) * time + phase);

			case OSC_SQR_ANA: //square fourier
				output = 0;
				for (int n = 1; n < 50; n += 1) {
					output += (sin((2 * (n + 1)) * w(frequency) * time + phase)) / (2 * (n + 1));
				}
				return output;

			case OSC_SQR_DIG: //square simple
				return sin(w(frequency) * time + phase) > 0 ? 1 : -1;

			case OSC_TRI: //triangle
				return asin(sin(w(frequency) * time + phase));

			case OSC_SAW_ANA: //saw fourier
				output = 0;
				for (int n = 1; n < 50; n++) {
					output -= (sin(n * w(frequency) * time + phase)) / n;
				}
				return output;

			case OSC_SAW_DIG: //saw simple
				return (fmod((w(frequency / 4) * time + phase), 2)) - 1;

			case OSC_NOISE: //noise TURNED OFF FOR NOW
				return 2.0 * ((double)rand() / (double)RAND_MAX) - 1;
		}
	}
};

class Lfo : public Osc {
public:
	double frequency;

	Lfo() {
		frequency = 1;
	}

	Lfo(double _frequency) {
		frequency = _frequency;
	}

	double getAmp(double time) {
		return Osc::getAmp(time, frequency);
	}
};

class Env {
public:
	double attackTime;
	double decayTime;
	double sustainAmp;
	double releaseTime;
	double noteOnTime;
	double noteOffTime;
	bool isNoteOn;

	Env() {
		attackTime = 0.01;
		decayTime = 0.01;
		sustainAmp = 0.8;
		releaseTime = 0.01;
		noteOnTime = 0.00;
		noteOffTime = 0.00;
		isNoteOn = false;
	}

	Env(double _attackTime, double _decayTime, double _sustainAmp, double _releaseTime) {
		attackTime = _attackTime;
		decayTime = _decayTime;
		sustainAmp = _sustainAmp;
		releaseTime = _releaseTime;
		noteOnTime = 0.00;
		noteOffTime = 0.00;
		isNoteOn = false;
	}

	void noteOn(double time) {
		noteOnTime = time;
		isNoteOn = true;
	}

	void noteOff(double time) {
		noteOffTime = time;
		isNoteOn = false;
	}

	double getAmp(double time) {
		double lifeTime = time - noteOnTime;

		// Attack
		if (lifeTime < attackTime) {
			return (lifeTime / attackTime);
		}
		// Decay
		if (lifeTime < attackTime + decayTime) {
			return 1 + ((sustainAmp - 1) * (lifeTime - attackTime) / releaseTime);
		}
		// Sustain
		if (isNoteOn) {
			return sustainAmp;
		}
		// Release
		if (time - noteOffTime < releaseTime) {
			return sustainAmp * (1 + ((noteOffTime - time) / releaseTime));
		}

		else {
			return 0;
		}
	}

	void operator = (Env env) {
		attackTime = env.attackTime;
		decayTime = env.decayTime;
		sustainAmp = env.sustainAmp;
		releaseTime = env.releaseTime;
	}
};

class channel {
public:
	int key;
	bool keyPressed;
	double frequencyOutput;
	static channel channels[4];
	Env env;

	channel() {
		key = -1;
		keyPressed = false;
		frequencyOutput = 0;
		env = Env();
	}
};

class Instrument {
public:
	Osc osc1;
	double osc1Amp;
	Osc osc2;
	double osc2Amp;
	double osc2Tune;
	Lfo lfo1;
	Lfo lfo2;
	Env env1;
	Env env2;
	int fmSource;
	double fmAmp;
	double tune;

	Instrument() {
		osc1 = Osc();
		osc1Amp = 1;
		osc2 = Osc();
		osc2Amp = 0;
		lfo1 = Lfo();
		lfo2 = Lfo();
		env1 = Env();
		env2 = Env();
		fmSource = SOURCE_LFO1;
		fmAmp = 0;
	}

	double getSourceAmp(int source, double time) {
		switch (source) {
		default:
			return 0;
		case SOURCE_LFO1:
			return lfo1.getAmp(time);
		case SOURCE_LFO2:
			return lfo2.getAmp(time);
		case SOURCE_ENV1:
			return env1.getAmp(time);
		case SOURCE_ENV2:
			return env2.getAmp(time);
		}
	}
};

double frequencyOutput = 0;
double rootOf2 = pow(2.0, 1.0 / 12.0);
double baseFrequency = 440;
channel channels[8] = { channel() };
const char noteLookup[16][3] = { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
Instrument voice;
std::string instrumentPath = "../";

double makeNoise(double time) {
	double output = 0;
	double amp;
	double fmOut;
	for (int i = 0; i < (sizeof(channels) / sizeof(channels[0])); i++) { // Calculate and sum each channel
		// Frequency Modulation Output
		fmOut = voice.fmAmp * voice.getSourceAmp(voice.fmSource, time);
		//std::wcout << fmOut;
		// Oscillator Output
		amp = voice.osc1Amp * voice.osc1.getAmp(time, channels[i].frequencyOutput * pow(rootOf2, voice.tune), fmOut)
			+ voice.osc2Amp * voice.osc2.getAmp(time, channels[i].frequencyOutput * pow(rootOf2, voice.osc2Tune + voice.tune), fmOut);
		//std::wcout << amp;
		// Env Output
		amp *= 0.05 * channels[i].env.getAmp(time);
		//std::wcout << amp;
		output += amp;
	}
	//std::wcout << output << std::endl;
	return output;
}

int availableChannel(double time) {
	//std::wcout << "availableChannel function: " << std::endl;
	int size = (sizeof(channels) / sizeof(channels[0]));
	for (int i = 0; i < (sizeof(channels) / sizeof(channels[0])); i++) {
		if (i > 0) {
			//std::wcout << "i: " << i << std::endl;
		}
		//std::wcout << "i: " << i << std::endl;
		if (time - channels[i].env.noteOffTime > channels[i].env.releaseTime and channels[i].key == -1) { //or channels[i].env.noteOffTime == 0.0) {
			//std::wcout << i << std::endl;
			return i;
		}
	}
	//std::wcout << "End of availableChannel function" << std::endl;
	return -1;
};





int main()
{
	// Get all sound hardware
	vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

	// Display findings
	for (int i = 0; i < devices.size(); i++) {
		std::wcout << devices[i] << std::endl;
	}
	std::wcout << "Using Device: " << devices[0] << std::endl;

	// Display a keyboard
	std::wcout << std::endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << std::endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << std::endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << std::endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << std::endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << std::endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << std::endl << std::endl << std::endl;


	// Create testing instrument

	voice = Instrument();
	voice.osc1.waveform = OSC_SIN;
	voice.osc2Amp = 0.01;
	voice.osc2.waveform = OSC_NOISE;
	voice.osc2Tune = -12;
	voice.tune = -12;
	voice.env1 = Env(0.10, 0.01, 1, 0.9);
	voice.fmAmp = 0.3;
	voice.lfo1.frequency = 5;

	// Create sound machine
	olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

	// Link noise function with sound machine
	sound.SetUserFunction(makeNoise);



	bool keyPressed = false;
	std::vector<int> currentKeys;
	while (true) {
		if (GetAsyncKeyState('S')  & GetAsyncKeyState(VK_CONTROL) & 0x8000) {
			// Write instrument to file
			fstream file;

			std::wcout << "\r                            \rEnter your instrument name: ";
			std::string instrumentName;
			std::cin >> instrumentName;
			file.open(instrumentPath + instrumentName + ".inst", ios::out, ios::_Noreplace);
			if (!file) {
				std::wcout << "\r                                                        \rError, that file already exists or something went wrong";
			}
			else {
				file << "osc1-wave " << voice.osc1.waveform << std::endl;
				file << "osc1-amp " << voice.osc1Amp << std::endl;
				file << "osc2-wave " << voice.osc2.waveform << std::endl;
				file << "osc2-amp " << voice.osc2Amp << std::endl;
				file << "osc2-tune " << voice.osc2Tune << std::endl;
				file << "lfo1-wave " << voice.lfo1.waveform << std::endl;
				file << "lfo1-freq " << voice.lfo1.frequency << std::endl;
				file << "lfo2-wave " << voice.lfo2.waveform << std::endl;
				file << "lfo2-freq " << voice.lfo2.frequency << std::endl;
				file << "env1-atk " << voice.env1.attackTime << std::endl;
				file << "env1-dec " << voice.env1.decayTime << std::endl;
				file << "env1-sus " << voice.env1.sustainAmp << std::endl;
				file << "env1-rel " << voice.env1.releaseTime << std::endl;
				file << "env2-atk " << voice.env2.attackTime << std::endl;
				file << "env2-dec " << voice.env2.decayTime << std::endl;
				file << "env2-sus " << voice.env2.sustainAmp << std::endl;
				file << "env2-rel " << voice.env2.releaseTime << std::endl;
				file << "fm-src " << voice.fmSource << std::endl;
				file << "fm-amp " << voice.fmAmp << std::endl;
				file.close();
				std::wcout << "Instrument saved" << std::endl; // zbmbzbmbm,
			}
		}

		if (GetAsyncKeyState('E') & GetAsyncKeyState(VK_CONTROL) & 0x8000) {
			// Open an instrument file and use as the voice
			fstream file;

			std::wcout << "\r                            \rEnter the instrument name: ";
			std::string instrumentName;
			std::cin >> instrumentName;
			file.open(instrumentPath + instrumentName + ".inst", ios::in);
			if (!file) {
				std::wcout << "\r                                                        \rError, that file already exists or something went wrong";
			}
			else {
				std::string instrumentText;
				file >> instrumentText;
				file >> instrumentText;
				voice.osc1.waveform = std::stoi(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.osc1Amp = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.osc2.waveform = std::stoi(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.osc2Amp = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.osc2Tune = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.lfo1.waveform = std::stoi(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.lfo1.frequency = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.lfo2.waveform = std::stoi(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.lfo2.frequency = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env1.attackTime = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env1.decayTime = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env1.sustainAmp = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env1.releaseTime = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env2.attackTime = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env2.decayTime = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env2.sustainAmp = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.env2.releaseTime = std::stod(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.fmSource = std::stoi(instrumentText);
				file >> instrumentText;
				file >> instrumentText;
				voice.fmAmp = std::stod(instrumentText);


				file.close();
			}

		}

		for (int i = 0; i < (sizeof(channels) / sizeof(channels[0])); i++) {
			channels[i].keyPressed = false;
		}
		for (int k = 0; k < 16; k++) {
			if (GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf")[k]) & 0x8000) { // check if key k is held down
				//std::wcout << k;
				if (std::find(currentKeys.begin(), currentKeys.end(), k) == currentKeys.end()) { // checks if there is a new keypress
					int channeli = availableChannel(sound.GetTime());
					//std::wcout << channeli << std::endl;
					if (channeli != -1) {
						//std::cout << channeli << " " << k << std::endl;
						channels[channeli].frequencyOutput = baseFrequency * pow(rootOf2, k);
						channels[channeli].key = k;
						channels[channeli].env = voice.env1;
						channels[channeli].env.noteOn(sound.GetTime());
						currentKeys.push_back(k);
					}
					//noteOn()
					//std::wcout << "\r                                        ";
					//std::wcout << "\rNote On: " << channels[channeli].frequencyOutput << "Hz" << " on channel " << channeli;
					std::wcout << "\r                            \rCurrent Keys: ";
					for (int i = 0; i < currentKeys.size(); i++) {
						std::wcout << noteLookup[currentKeys[i]] << " ";
					}

					//for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
					//	std::wcout << std::endl << "Channel " << i << " |  key: " << channels[i].key << "  keyPressed: " << channels[i].keyPressed;
					//}
					//std::wcout << std::endl;
				}
				for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
					if (channels[i].key == k) {
						channels[i].keyPressed = true;
						//std::wcout << "Setting key " << k << " in channel " << i << " as pressed" << std::endl;
					}
				}
			}
		}
		for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
			if (!channels[i].keyPressed) {
				//std::wcout << channels[i].key;
				if (channels[i].key != -1) {
					//std::wcout << "Key: " << channels[i].key << std::endl;
				}
				if (std::find(currentKeys.begin(), currentKeys.end(), channels[i].key) != currentKeys.end()) {
					// noteOff()
					//std::wcout << "AA" << std::endl;
					//std::wcout << "About to remove: " << channels[i].key << std::endl;
					channels[i].env.noteOff(sound.GetTime());
					currentKeys.erase(std::remove(currentKeys.begin(), currentKeys.end(), channels[i].key), currentKeys.end());
					channels[i].key = -1;
					std::wcout << "\r                            \rCurrent Keys: ";
					for (int i = 0; i < currentKeys.size(); i++) {
						std::wcout << noteLookup[currentKeys[i]] << " ";
					}
					//for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
					//	std::wcout << std::endl << "Channel " << i << " |  key: " << channels[i].key << "  keyPressed: " << channels[i].keyPressed;
					//}
					//std::wcout << std::endl;
				}
			}
		}
	}
}