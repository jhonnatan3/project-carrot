/*****************************************************************************
 ** PROJECT CARROT - MAIN CLASS                                             **
 *****************************************************************************/

#pragma once
#include "Version.h"

#include <memory>
#include <functional>
#include <QMainWindow>
#include <QCloseEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QTimer>
#include <QTime>
#include <QString>
#include <QWidget>
#include <QObject>
#include <QMap>
#include <QVector>
#include <QStack>
#include <bass.h>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>

#include "ui_CarrotQt5.h"
#include "graphics/BitmapFont.h"
#include "graphics/BitmapString.h"
#include "graphics/CarrotCanvas.h"
#include "graphics/ShaderSource.h"
#include "gamestate/ControlManager.h"
#include "gamestate/ResourceManager.h"
#include "struct/Resources.h"
#include "struct/NextLevelData.h"
#include "struct/DebugConfig.h"

class MenuScreen;
class EngineState;
class LevelManager;
class EventSpawner;

class CarrotQt5 : public QMainWindow, public std::enable_shared_from_this<CarrotQt5> {

    Q_OBJECT

public:
    CarrotQt5(QWidget *parent = 0);
    ~CarrotQt5();
    void parseCommandLine();
    void startMainMenu();
    unsigned long getFrame();
    CarrotCanvas* getCanvas();
    SoundSystem* getSoundSystem();
    std::shared_ptr<BitmapFont> getFont(BitmapFontSize size = NORMAL);
    std::shared_ptr<ResourceSet> loadActorTypeResources(const QString& actorType);
    float getCurrentFPS();
    EventSpawner* getEventSpawner();

    template<typename T, typename... P>
    void pushState(bool replace, P... params);

    void popState();

#ifdef CARROT_DEBUG
    DebugConfig debugConfig;
    DebugConfig* getDebugConfig();
#endif

    void startGame(const NextLevelData carryOver = { "" });
    void tryQuit();
    
protected:
    bool eventFilter(QObject* watched, QEvent* e);

protected slots:
    void closeEvent(QCloseEvent* event);
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);
    void resizeEvent(QResizeEvent* event);

private:
    Ui::CarrotQt5Class ui;
    QTimer myTimer;
    bool initialized;
    std::shared_ptr<CarrotCanvas> windowCanvas;
    std::shared_ptr<BitmapFont> smallFont;
    std::shared_ptr<BitmapFont> mainFont;
    std::shared_ptr<BitmapFont> largeFont;
    std::unique_ptr<ResourceManager> resourceManager;
    std::shared_ptr<ControlManager> controlManager;
    unsigned long frame;

    // Shared pointers, but never given to other objects, so effectively unique
    // (Qt containers cannot hold unique pointers)
    QStack<std::shared_ptr<EngineState>> stateStack;

    void pushState(std::shared_ptr<EngineState> state);

    std::function<void()> afterTickCallback;
    QTime lastTimestamp;
    float fps;

    // The event spawner is related to the gameplay, but it is located in this class
    // mainly to prevent the need to reload it on every new level.
    std::unique_ptr<EventSpawner> eventSpawner;

private slots:
    void tick();
    void openAboutCarrot();
    void openHomePage();
#ifdef CARROT_DEBUG
    void debugLoadMusic();
    void debugShowMasks(bool show);
    void debugSetOverlaysActive(bool active);
#endif
};

template<typename T, typename... P>
inline void CarrotQt5::pushState(bool replace, P... params) {
    if (replace) {
        stateStack.pop();
    }

    pushState(std::make_shared<T>(this, params...));
}
