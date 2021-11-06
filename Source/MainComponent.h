#pragma once

#include <JuceHeader.h>
using namespace juce;


class Visualiser : public AudioVisualiserComponent
{
public:
    Visualiser() : AudioVisualiserComponent(2)
    {
        setBufferSize(512);
        setSamplesPerBlock(256);
        setColours(Colours::black, Colours::white);
    }
};

#include "filmStrip.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    //===============================================================================
    void updateTextLabel(float samplingFrequency, float numChannels, String Name, String formatType);
    void loadFiles(String stringToShow); 
    void processData();
    void adjustGainLabel();
    void adjustDryWetLabel();

    dsp::Convolution::Stereo isStereo;
    Visualiser waveVisualiser;
    AudioBuffer<float> AudioBufferL;
    AudioBuffer<float> AudioBufferR;
    std::atomic<float> Gain, Dry, Wet;
private:
    
    ImageComponent myImageComponent;
    AudioFormatManager formatManager, formatManager2;

    std::unique_ptr<TextButton> loadButton, processDataButton;
    std::unique_ptr<AlertWindow> errorMessage;
    std::unique_ptr<Label> fsTextLabel, channelsTextLabel, nameTextLabel, formatTextLabel, dryWetLabel, dryWetLabelTitle, gainLabel, gainLabelTitle;;
    std::unique_ptr<FilmStripKnob>  dryWetSlider, gainSlider;

    dsp::Convolution convolutionEngine, convolutionEngine1;
    dsp::ProcessSpec convolutionProperties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
