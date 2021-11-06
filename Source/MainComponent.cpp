#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() :
    isStereo(dsp::Convolution::Stereo::no), Dry(0.5), Wet(0.5), Gain(-13)
{

    Image  knobImage;
    knobImage = GIFImageFormat::loadFrom(BinaryData::Knob_png, BinaryData::Knob_pngSize);

    auto logo = ImageCache::getFromMemory(BinaryData::Logo_PNG, BinaryData::Logo_PNGSize);
    myImageComponent.setImage(logo, RectanglePlacement::stretchToFit);

    if (!logo.isNull())
    {
        myImageComponent.setImage(logo, RectanglePlacement::stretchToFit);
    }      
    else
    {
        jassert(!logo.isNull());
    }

    addAndMakeVisible(myImageComponent);

    loadButton.reset(new TextButton());
    addAndMakeVisible(loadButton.get());
    loadButton->setButtonText("Load Impulse Response");
    loadButton->onClick = [this] {loadFiles("Load Impulse Response"); };
    
    nameTextLabel.reset(new Label());
    addAndMakeVisible(nameTextLabel.get());
    nameTextLabel->setFont(Font(16, Font::bold));

    formatTextLabel.reset(new Label());
    addAndMakeVisible(formatTextLabel.get());
    formatTextLabel->setFont(Font(16, Font::bold));

    channelsTextLabel.reset(new Label());
    addAndMakeVisible(channelsTextLabel.get());
    channelsTextLabel->setFont(Font(16, Font::bold));

    fsTextLabel.reset(new Label());
    addAndMakeVisible(fsTextLabel.get());
    fsTextLabel->setFont(Font(16, Font::bold));

    addAndMakeVisible(waveVisualiser);
    updateTextLabel( 0, 0, " ", " ");

    processDataButton.reset(new TextButton());
    addAndMakeVisible(processDataButton.get());
    processDataButton->setButtonText("Start");
    processDataButton->onClick = [this] {processData(); };

    dryWetSlider.reset(new FilmStripKnob(knobImage, 45, false));
    addAndMakeVisible(dryWetSlider.get());

    dryWetLabel.reset(new Label());
    addAndMakeVisible(dryWetLabel.get());

    dryWetSlider->setRange(0, 1, 0.01);
    dryWetSlider->setSliderStyle(Slider::SliderStyle::Rotary);
    dryWetSlider->setColour(Slider::ColourIds::rotarySliderFillColourId, Colour(Colours::blue));
    dryWetSlider->setColour(Slider::ColourIds::thumbColourId, Colour(Colours::darkorange));
    dryWetSlider->setValue(0.5);

    dryWetSlider->onValueChange = [this]
    {
        float DryWet = dryWetSlider->getValue();
        Wet = DryWet;
        dryWetLabel->setText(String(dryWetSlider->getValue()*100, 2) + " %", dontSendNotification);
        adjustDryWetLabel();
    };

    dryWetLabelTitle.reset(new Label());
    addAndMakeVisible(dryWetLabelTitle.get());
    dryWetLabelTitle->setText("Dry & Wet", dontSendNotification);
    dryWetLabelTitle->setColour(Label::textColourId, Colours::grey);
    dryWetLabelTitle->setJustificationType(Justification::centred);


    gainSlider.reset(new FilmStripKnob(knobImage, 45, false));
    addAndMakeVisible(gainSlider.get());

    gainLabel.reset(new Label());
    addAndMakeVisible(gainLabel.get());

    gainSlider->setRange(-80, 0, 0.01);
    gainSlider->setSliderStyle(Slider::SliderStyle::Rotary);
    gainSlider->setColour(Slider::ColourIds::rotarySliderFillColourId, Colour(Colours::blue));
    gainSlider->setColour(Slider::ColourIds::thumbColourId, Colour(Colours::darkorange));
    gainSlider->setValue(-13);

    gainSlider->onValueChange = [this]
    {
        float decJ = Decibels::decibelsToGain(gainSlider->getValue());
        Gain = decJ;
        gainLabel->setText(String(gainSlider->getValue(), 2) + " dB", dontSendNotification);
        adjustGainLabel();
    };


    gainLabelTitle.reset(new Label());
    addAndMakeVisible(gainLabelTitle.get());
    gainLabelTitle->setText("Gain", dontSendNotification);
    gainLabelTitle->setColour(Label::textColourId, Colours::grey);
    gainLabelTitle->setJustificationType(Justification::centred);

    // Make sure you set the size of the component after
    // you add any child components.

    setSize (420, 600);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }

    formatManager.registerBasicFormats();

    auto AudioSetup = deviceManager.getAudioDeviceSetup();

    AudioSetup.bufferSize = 1024;
    AudioSetup.sampleRate = 44100;
    deviceManager.setAudioDeviceSetup(AudioSetup, true);
    //DBG(AudioSetup.bufferSize);
    //DBG(AudioSetup.sampleRate);
}

MainComponent::~MainComponent()
{
    loadButton = nullptr;
    fsTextLabel = nullptr;
    errorMessage = nullptr;
    channelsTextLabel = nullptr;
    nameTextLabel = nullptr;
    formatTextLabel = nullptr;
    processDataButton = nullptr;
    dryWetSlider = nullptr;
    gainSlider = nullptr;
    dryWetLabel = nullptr;
    dryWetLabelTitle = nullptr;
    gainLabel = nullptr;
    gainLabelTitle = nullptr;

    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    AudioBufferL.setSize(1, samplesPerBlockExpected);
    AudioBufferR.setSize(1, samplesPerBlockExpected);

    convolutionProperties.sampleRate = sampleRate;
    convolutionProperties.maximumBlockSize = samplesPerBlockExpected;
    convolutionProperties.numChannels = 2;
    convolutionEngine.prepare(convolutionProperties);
    convolutionEngine.reset();
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    Dry = 1 - Wet;

    if (processDataButton->getToggleState())
    {

        AudioBufferL.copyFrom(0, 0, bufferToFill.buffer->getReadPointer(0), bufferToFill.buffer->getNumSamples());
        AudioBufferR.copyFrom(0, 0, bufferToFill.buffer->getReadPointer(1), bufferToFill.buffer->getNumSamples());
        
        dsp::AudioBlock<float> tempAudioBlock(*bufferToFill.buffer);
        dsp::ProcessContextReplacing<float> ctx(tempAudioBlock);

        convolutionEngine.process(ctx);

        for (auto sample = 0; sample < bufferToFill.buffer->getNumSamples(); ++sample)
        {
            for (auto channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
            {
                if (channel == 0)
                {
                    float* channelN = bufferToFill.buffer->getWritePointer(channel);

                    channelN[sample] = ((Wet*channelN[sample]) + (Dry*AudioBufferL.getSample(0, sample)))*Gain;
                }

                else if (channel == 1)
                {
                    float* channelN = bufferToFill.buffer->getWritePointer(channel);
                    channelN[sample] = ((Wet*channelN[sample]) + (Dry*AudioBufferR.getSample(0,sample))) * Gain;
                }
                else
                {
                    bufferToFill.clearActiveBufferRegion();
                }

            }
        }
    }
    else
    {
        bufferToFill.clearActiveBufferRegion();
    }

        waveVisualiser.pushBuffer(*bufferToFill.buffer);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll(Colours::black);

    g.setColour(Colour(Colours::grey));
    g.drawRect(10, 10, 400, 580);

    g.setColour(Colour(Colours::white));
    g.drawRect(35, 345, 350, 195);

}

void MainComponent::resized()
{
    myImageComponent.setBounds(130, 15, 140, 110);
    loadButton->setBounds(127, 130, 150, 24);
    nameTextLabel->setBounds(30, 204, 344, 24);
    formatTextLabel->setBounds(30, 224, 344, 24);
    channelsTextLabel->setBounds(30, 244, 344, 24);
    fsTextLabel->setBounds(30, 264, 344, 24);
    waveVisualiser.setBounds(40, 354, 340, 180);
    processDataButton->setBounds(35, 550, 350, 25);

    dryWetSlider->setBounds(240, 220, 66, 66);
    dryWetLabel->setBounds(248, 255, 66, 66);
    dryWetLabelTitle->setBounds(242, 175, 66, 66);

    gainSlider->setBounds(320, 220, 66, 66);
    gainLabel->setBounds(223, 255, 66, 66);
    gainLabelTitle->setBounds(320, 175, 66, 66);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MainComponent::loadFiles(String stringToShow)
{
    File newFile;
    FileChooser newFileChooser(stringToShow, File::getSpecialLocation(File::userDesktopDirectory), "*.wav");

    if (newFileChooser.browseForFileToOpen())
    {
        newFile = newFileChooser.getResult();
        std::unique_ptr<AudioFormatReader> formatReader;
        formatReader.reset(formatManager.createReaderFor(newFile));
        isStereo = formatReader->numChannels >= 2 ? dsp::Convolution::Stereo::yes : dsp::Convolution::Stereo::no;
        convolutionEngine.loadImpulseResponse(newFile, isStereo, dsp::Convolution::Trim::no ,0);
        updateTextLabel(formatReader->sampleRate, formatReader->numChannels, newFile.getFileName(), formatReader->getFormatName());
        convolutionEngine.reset();
    }
    else
    {
        errorMessage->showMessageBoxAsync(AlertWindow::WarningIcon, "Error", "Ningun Archivo Seleccionado", "Ok", nullptr);
    }
}

void MainComponent::updateTextLabel(float samplingFrequency, float numChannels, String Name, String formatType)
{
    String newSamplingFrequency, newNumChannels, newName, newFormatType;

    newSamplingFrequency = "S.Rate: " + String(samplingFrequency) + " Hz";
    newNumChannels = "Channels: " + String(numChannels);
    newName = "Name: " + Name;
    newFormatType = "Format: " + formatType;

    fsTextLabel -> setText(newSamplingFrequency, dontSendNotification);
    channelsTextLabel -> setText(newNumChannels, dontSendNotification);
    nameTextLabel ->setText(newName, dontSendNotification);
    formatTextLabel ->setText(newFormatType, dontSendNotification);
}


void MainComponent::processData()
{
    if (processDataButton->getToggleState())
    {
        processDataButton->setToggleState(false, dontSendNotification);
        processDataButton->setButtonText("Start");
    }
    else
    {
        processDataButton->setToggleState(true, dontSendNotification);
        processDataButton->setButtonText("Stop");
    }

    repaint();
}

void MainComponent::adjustGainLabel()
{
    if (gainSlider->getValue() == 0) { gainLabel->setBounds(328, 255, 66, 66); }
    else if (gainSlider->getValue() > -10) { gainLabel->setBounds(324, 255, 66, 66); }
    else { gainLabel->setBounds(323, 255, 66, 66); }
    repaint();
}

void MainComponent::adjustDryWetLabel()
{
    if (dryWetSlider->getValue() < 0.1) { dryWetLabel->setBounds(253, 255, 66, 66); }
    else if (dryWetSlider->getValue() == 1) { dryWetLabel->setBounds(247, 255, 66, 66); }
    else { dryWetLabel->setBounds(248, 255, 66, 66); }
    repaint();
}

