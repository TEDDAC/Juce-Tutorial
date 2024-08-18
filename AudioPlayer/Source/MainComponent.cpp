#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (300, 200);

    // Some platforms require permissions to open input channels so request that here
    //if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
    //    && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    //{
    //    juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
    //                                       [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    //}
    //else
    //{
    //    // Specify the number of input and output channels that we want to open
    //    setAudioChannels (2, 2);
    //}


    addAndMakeVisible(&openButton);
    openButton.setButtonText("Open...");
    openButton.onClick = [this] { openButtonClicked(); };

    addAndMakeVisible(&playButton);
    playButton.setButtonText("Play");
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setEnabled(false);

    addAndMakeVisible(&stopButton);
    stopButton.setButtonText("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.setEnabled(false);

    addAndMakeVisible(&timeLabel);
    timeLabel.setText("0:00", juce::dontSendNotification);



    formatManager.registerBasicFormats();
    transportSource.addChangeListener(this);

    state = Stopped;

    // Sert � configurer l'audio, est obligatoire
    setAudioChannels(2, 2);
        
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource.get() == nullptr) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    transportSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    openButton.setBounds(10, 10, getWidth() - 20, 20);
    playButton.setBounds(10, 40, getWidth() - 20, 20);
    stopButton.setBounds(10, 70, getWidth() - 20, 20);
    timeLabel.setBounds(10, 100, getWidth() - 20, 20);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transportSource) {
        if (transportSource.isPlaying()) {
            changeState(Playing);
        }
        else if(state == Stopping || state == Playing)
        {
            changeState(Stopped);
        }
        else if (state == Pausing)
        {
            changeState(Paused);
        }
    }
}

void MainComponent::timerCallback()
{
    if (transportSource.isPlaying()) {
        double currentPosition = transportSource.getCurrentPosition();
        int minutes = (int)currentPosition / 60;
        int seconds = (int)currentPosition % 60;
        juce::String text = juce::String::formatted("%02d:%02d", minutes, seconds);
        timeLabel.setText(text, juce::dontSendNotification);
    }
}

void MainComponent::changeState(TransportState newState)
{
    if (state != newState)
    {
        state = newState;

        switch (state)
        {
        case Stopped:
            playButton.setButtonText("Play");
            stopButton.setButtonText("Stop");
            stopButton.setEnabled(false);
            transportSource.setPosition(0.0);
            break;

        case Starting:
            transportSource.start();
            break;

        case Playing:
            playButton.setButtonText("Pause");
            stopButton.setButtonText("Stop");
            stopButton.setEnabled(true);
            startTimer(1000);
            break;

        case Pausing:
            transportSource.stop();
            stopTimer();
            break;

        case Paused:
            playButton.setButtonText("Resume");
            stopButton.setButtonText("Return to Zero");
            break;

        case Stopping:
            transportSource.stop();
            stopTimer();
            break;
        }
    }
}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Select a Wave file to play...", juce::File{}, "*.wav");
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) 
        {
            // Quand on choisi un fichier alors que la source est encore en lecture, �a fait planter l'application.
            // On s'assure donc de stopper la source avant de la modifier.
            if (transportSource.isPlaying()) {
                changeState(Stopping);
            }

            auto file = fc.getResult();
            if (file != juce::File{})
            {
                auto* reader = formatManager.createReaderFor(file);

                if (reader != nullptr)
                {
                    auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                    transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
                    playButton.setEnabled(true);
                    readerSource.reset(newSource.release());
                }
            }
        });
}

void MainComponent::playButtonClicked()
{
    if (state == Stopped || state == Paused) 
    {
        changeState(Starting);
    }
    else if (state == Playing) 
    {
        changeState(Pausing);
    }
}

void MainComponent::stopButtonClicked()
{
    if (state == Paused)
    {
        changeState(Stopped);
    }
    else
    {
        changeState(Stopping);
    }
}
