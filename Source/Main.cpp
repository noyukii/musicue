#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "WorkspaceLauncherComponent.h"

class MusiCueApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MusiCue"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void systemRequestedQuit() override;

    void initialise(const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MusiCue";
        options.folderName = "MusiCue";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters(options);

        mainWindow = std::make_unique<MainWindow>(getApplicationName(),
                                                  *appProperties.getUserSettings());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        class LauncherWindow : public juce::DocumentWindow
        {
        public:
            LauncherWindow(juce::PropertiesFile& props, MainWindow& owner)
                : DocumentWindow("MusiCue Workspace Launcher", juce::Colour(0xff242424),
                                 DocumentWindow::closeButton), mainWindow(owner), launcher(props)
            {
                setUsingNativeTitleBar(true);
                setLookAndFeel(&lookAndFeel);
                launcher.setLookAndFeel(&lookAndFeel);
                launcher.onNewWorkspace = [this] { openNewWorkspace(); };
                launcher.onOpenWorkspace = [this] { chooseWorkspace(); };
                launcher.onOpenRecent = [this](const juce::File& file) { openWorkspace(file); };
                setContentNonOwned(&launcher, false);
                setResizable(true, true);
                setResizeLimits(760, 480, 1000, 720);
                centreWithSize(820, 520);
                setVisible(true);
            }

            ~LauncherWindow() override
            {
                launcher.setLookAndFeel(nullptr);
                setLookAndFeel(nullptr);
            }

            void closeButtonPressed() override
            {
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }

        private:
            void openNewWorkspace()
            {
                mainWindow.openNewWorkspace();
                closeLauncher();
            }

            void chooseWorkspace()
            {
                chooser = std::make_unique<juce::FileChooser>(
                    "Open Workspace", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                    "*.musicue");
                chooser->launchAsync(juce::FileBrowserComponent::openMode
                                         | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fileChooser)
                    {
                        if (const auto file = fileChooser.getResult(); file != juce::File())
                            openWorkspace(file);
                        chooser.reset();
                    });
            }

            void openWorkspace(const juce::File& file)
            {
                mainWindow.openWorkspace(file);
                closeLauncher();
            }

            void closeLauncher()
            {
                setVisible(false);
                auto* owner = &mainWindow;
                juce::Timer::callAfterDelay(1, [owner] { owner->closeLauncher(); });
            }

            MainWindow& mainWindow;
            MusiCueLookAndFeel lookAndFeel;
            WorkspaceLauncherComponent launcher;
            std::unique_ptr<juce::FileChooser> chooser;
        };

        MainWindow(const juce::String& name, juce::PropertiesFile& props)
            : DocumentWindow(name + " - Workspace Launcher",
                             juce::Colour(0xff242424), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            mainComponent = new MainComponent(props);
            setContentOwned(mainComponent, true);
            setResizable(true, true);
            setResizeLimits(1000, 640, 10000, 10000);
            centreWithSize(1150, 760);
            setVisible(false);
            launcherWindow = std::make_unique<LauncherWindow>(props, *this);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void requestQuit()
        {
            mainComponent->requestWorkspaceClose([]
            {
                juce::JUCEApplication::getInstance()->quit();
            });
        }

        void openNewWorkspace()
        {
            mainComponent->createNewWorkspace();
            setVisible(true);
            toFront(true);
        }

        void openWorkspace(const juce::File& file)
        {
            mainComponent->loadWorkspace(file);
            setVisible(true);
            toFront(true);
        }

        void closeLauncher()
        {
            launcherWindow.reset();
        }

    private:
        MainComponent* mainComponent = nullptr;
        std::unique_ptr<LauncherWindow> launcherWindow;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
    juce::ApplicationProperties appProperties;
};

void MusiCueApplication::systemRequestedQuit()
{
    if (mainWindow != nullptr)
        mainWindow->requestQuit();
    else
        quit();
}

START_JUCE_APPLICATION(MusiCueApplication)
