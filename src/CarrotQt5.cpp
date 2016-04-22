#include "CarrotQt5.h"
#include "actor/CommonActor.h"
#include "actor/Player.h"
#include "actor/SolidObject.h"
#include "gamestate/EventMap.h"
#include "gamestate/TileMap.h"
#include "graphics/QSFMLCanvas.h"
#include "graphics/CarrotCanvas.h"
#include "menu/MenuScreen.h"
#include "JJ2Format.h"

#include "ui_AboutCarrotDialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QTimer>
#include <QTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QFile>
#include <QDebug>
#include <bass.h>

CarrotQt5::CarrotQt5(QWidget *parent) : QMainWindow(parent),
#ifdef CARROT_DEBUG
    currentTempModifier(0), dbgShowMasked(false), dbgOverlaysActive(true),
#endif
    paused(false), levelName(""), frame(0), gravity(0.3), lightingLevel(80), isMenu(false), menuObject(nullptr), fps(0) {
#ifndef CARROT_DEBUG
    // Set application location as the working directory
    QDir::setCurrent(QCoreApplication::applicationDirPath());
#endif

    // Apply the UI file, set window flags and connect menu items to class slots
    ui.setupUi(this);
    //setWindowFlags( (windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowMaximizeButtonHint);

    connect(ui.action_About_Project_Carrot, SIGNAL(triggered()), this, SLOT(openAboutCarrot()));
    connect(ui.action_About_Qt_5, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(ui.action_Home_Page, SIGNAL(triggered()), this, SLOT(openHomePage()));


#ifdef CARROT_DEBUG
    for (int i = 0; i < 32; ++i) {
        tempModifierName[i] = "(UNSET)";
        tempModifier[i] = 0;
    }
    tempModifierName[0] = "PerspCurve";      tempModifier[0] = 20;
    tempModifierName[1] = "PerspMultipNear"; tempModifier[1] = 1;
    tempModifierName[2] = "PerspMultipFar";  tempModifier[2] = 3;
    tempModifierName[3] = "PerspSpeed";      tempModifier[3] = 4;
    tempModifierName[4] = "SkyDepth";        tempModifier[4] = 1;

    connect(ui.debug_music, SIGNAL(triggered()), this, SLOT(debugLoadMusic()));
    connect(ui.debug_gravity, SIGNAL(triggered()), this, SLOT(debugSetGravity()));
    connect(ui.debug_lighting, SIGNAL(triggered()), this, SLOT(debugSetLighting()));
    connect(ui.debug_masks, SIGNAL(triggered(bool)), this, SLOT(debugShowMasks(bool)));
    connect(ui.debug_position, SIGNAL(triggered()), this, SLOT(debugSetPosition()));
    connect(ui.debug_overlays, SIGNAL(triggered(bool)), this, SLOT(debugSetOverlaysActive(bool)));
#else
    ui.menuDebug->menuAction()->setVisible(false);
#endif

    // Initialize the paint surface
    windowCanvas = std::make_shared<CarrotCanvas>(ui.mainFrame, QPoint(0, 0), QSize(800, 600));
    windowCanvas->show();
    
    // Fill the player pointer table with zeroes
    std::fill_n(players, 32, nullptr);

    // Read the main font
    mainFont = std::make_shared<BitmapFont>("Data/Assets/ui/font_medium.png", 29, 31, 1, 224, 32, 256);
    
    // Define the game view which we'll use for following the player
    gameView = std::make_unique<sf::View>(sf::FloatRect(0.0, 0.0, 800.0, 600.0));
    uiView = std::make_unique<sf::View>(sf::FloatRect(0.0, 0.0, 800.0, 600.0));

    resourceManager = std::make_unique<ResourceManager>();
    controlManager = std::make_shared<ControlManager>();

    ShaderSource::initialize();

    installEventFilter(this);

    // Define pause screen resources
    pausedScreenshot = std::make_unique<sf::Texture>();
    pausedScreenshot->create(800, 600);
    pausedScreenshot->update(*windowCanvas);

    pausedScreenshotSprite = std::make_unique<sf::Sprite>();
    pausedScreenshotSprite->setTexture(*pausedScreenshot);

    // Define the pause text and add vertical bounce animation to it
    pausedText = std::make_unique<BitmapString>(mainFont, "Pause", FONT_ALIGN_CENTER);
    pausedText->setAnimation(true, 0.0, 6.0, 0.015, 1.25);

    // Create the light overlay texture
    lightTexture = std::make_unique<sf::RenderTexture>();
    lightTexture->create(1600, 1200);

    // Fill the light overlay texture with black
    sf::RectangleShape tempOverlayRectangle(sf::Vector2f(1600, 1200));
    tempOverlayRectangle.setFillColor(sf::Color::Black);
    lightTexture->draw(tempOverlayRectangle);

    // Create a hole in the middle
    sf::CircleShape tempOverlayCircle(96);
    tempOverlayCircle.setFillColor(sf::Color(0,0,0,0));
    tempOverlayCircle.setOrigin(96,96);
    tempOverlayCircle.setPosition(800, 600);
    lightTexture->draw(tempOverlayCircle, sf::RenderStates(sf::BlendNone));

    // TODO: create a feather effect
    // check the shader stuff and apply them to the texture here

    lastTimestamp = QTime::currentTime();
}

CarrotQt5::~CarrotQt5() {
    if (!isMenu) {
        cleanUpLevel();
    }

    ShaderSource::teardown();
}

void CarrotQt5::parseCommandLine() {
    QStringList param = QCoreApplication::arguments();
    QString level = "";
    if (param.size() > 1) {
        QDir dir(QDir::currentPath());
        if (dir.exists(param.at(param.size() - 1))) {
            level = param.at(param.size() - 1);
        }
    }

    if (level != "") {
        startGame(level);
    } else {
        startMainMenu();
    }
}

void CarrotQt5::cleanUpLevel() {
    actors.clear();
    std::fill_n(players, 32, nullptr);
    resourceManager->getGraphicsCache()->flush();
    
    windowCanvas->setView(*uiView);
}

void CarrotQt5::startGame(QVariant filename) {
    // Parameter is expected to be a string
    if (filename.canConvert<QString>()) {
        if (loadLevel(filename.toString())) {
            myTimer.setInterval(1000 / 70);
            myTimer.disconnect(this, SLOT(mainMenuTick()));
            connect(&myTimer, SIGNAL(timeout()), this, SLOT(gameTick()));
            menuObject = nullptr;

            myTimer.start();
            isMenu = false;
        }
    } else {
        // invalid call
    }
}

void CarrotQt5::startMainMenu() {
    myTimer.setInterval(1000 / 70);
    myTimer.disconnect(this, SLOT(gameTick()));
    connect(&myTimer, SIGNAL(timeout()), this, SLOT(mainMenuTick()));
    myTimer.start();
    isMenu = true;

    setWindowTitle("Project Carrot");
    resourceManager->getSoundSystem()->setMusic("Music/Menu.it");

    menuObject = std::make_unique<MenuScreen>(shared_from_this());
}

void CarrotQt5::openAboutCarrot() {
    QDialog* dialog = new QDialog(this, 0);
    Ui_AboutCarrot ui;
    ui.setupUi(dialog);

    dialog->show();
}

void CarrotQt5::openHomePage() {
    QDesktopServices::openUrl(QUrl("http://www.jazz2online.com/jcf/showthread.php?t=19535"));
}

/*void CarrotQt5::openHelp() {
    QDesktopServices::openUrl(QUrl("http://www.google.com/"));
}*/

void CarrotQt5::closeEvent(QCloseEvent *event) {
    QMessageBox msg;
    msg.setWindowTitle("Quit Project Carrot?");
    msg.setText("Are you sure you want to quit Project Carrot?");
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setDefaultButton(QMessageBox::No);
    msg.setIcon(QMessageBox::Question);

    if (msg.exec() == QMessageBox::No) {
        event->ignore();
    }
}

bool CarrotQt5::eventFilter(QObject *watched, QEvent *e) {
    // Catch focus events to mute the music when the window doesn'loadingScreenTexture have it
    if (e->type() == QEvent::WindowActivate) {
        resourceManager->getSoundSystem()->fadeMusicIn(1000);
        paused = false;
    } else if (e->type() == QEvent::WindowDeactivate) {
        resourceManager->getSoundSystem()->fadeMusicOut(1000);
        paused = true;
        windowCanvas->setView(*uiView);
        pausedScreenshot->update(*windowCanvas);
    } else if (e->type() == QEvent::Resize) {
        int w = ui.centralWidget->size().width();
        int h = ui.centralWidget->size().height();

        gameView->setSize(w, h);
        uiView->setSize(w, h);
        uiView->setCenter(w / 2.0, h / 2.0);
        ui.mainFrame->resize(ui.centralWidget->size());
        windowCanvas->setSize(sf::Vector2u(w, h));
        windowCanvas->setView(*uiView);
        pausedScreenshot->create(w, h);
    }
    return FALSE;  // dispatch normally
}

void CarrotQt5::processControlEvents() {
    ControlEventList events = controlManager->getPendingEvents();

    foreach(auto pair, events.controlDownEvents) {
        if (isMenu) {
            if (menuObject != nullptr) {
                menuObject->processControlDownEvent(pair);
            }
        } else {
            foreach(auto& actor, actors) {
                actor->processControlDownEvent(pair);
            }
        }
    }

    if (isMenu) {
        foreach(auto key, events.controlHeldEvents.keys()) {
            if (menuObject != nullptr) {
                menuObject->processControlHeldEvent(qMakePair(key, events.controlHeldEvents.value(key)));
            }
        }
    } else {
        foreach(auto& actor, actors) {
            actor->processAllControlHeldEvents(events.controlHeldEvents);
        }
    }

    foreach(auto pair, events.controlUpEvents) {
        if (isMenu) {
            if (menuObject != nullptr) {
                menuObject->processControlUpEvent(pair);
            }
        } else {
            foreach(auto& actor, actors) {
                actor->processControlUpEvent(pair);
            }
        }
    }

    controlManager->processFrame();
}

void CarrotQt5::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return;
    }

    if (!isMenu) {
        if (event->key() == Qt::Key::Key_Escape) {
            cleanUpLevel();
            startMainMenu();
        }

#ifdef CARROT_DEBUG
        if (event->key() == Qt::Key::Key_Insert) {
            tempModifier[currentTempModifier] += 1;
        }
        if (event->key() == Qt::Key::Key_Delete) {
            tempModifier[currentTempModifier] -= 1;
        }
        if (event->key() == Qt::Key::Key_PageUp) {
            currentTempModifier = (currentTempModifier + 1) % 32;
        }
        if (event->key() == Qt::Key::Key_PageDown) {
            currentTempModifier = (currentTempModifier + 31) % 32;
        }
#endif
    }

    controlManager->setControlHeldDown(event->key());
}

void CarrotQt5::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return;
    }

    controlManager->setControlReleased(event->key());
}

void CarrotQt5::mainMenuTick() {
    frame++;
    processControlEvents();
    
    // Clear the drawing surface
    windowCanvas->clear();

    if (menuObject != nullptr) {
        menuObject->tickEvent();
    }
    
    // Update the drawn surface to the screen
    windowCanvas->updateContents();
}

void CarrotQt5::gameTick() {
    frame++;
    if (frame % 20 == 0) {
        QTime now = QTime::currentTime();
        fps = (1000.0 / (lastTimestamp.msecsTo(now) / 20.0));
        lastTimestamp = now;
    }

    if (paused) {
        // Set up a partially translucent black overlay
        sf::RectangleShape overlay(sf::Vector2f(800.0, 600.0));
        overlay.setFillColor(sf::Color(0, 0, 0, 120));

        windowCanvas->draw(*pausedScreenshotSprite);
        windowCanvas->draw(overlay);

        // Draw the pause string
        pausedText->drawString(getCanvas(), 400, 280);

        // Update the display
        windowCanvas->updateContents();
        return;
    }

    // Clear the drawing surface; we don't want to do this if we emulate the JJ2 behavior
    windowCanvas->clear();

    // Set player to the center of the view
    players[0]->setToViewCenter();
    windowCanvas->setView(*gameView);

    // Deactivate far away instances, create near instances
    int view_x = static_cast<unsigned>(windowCanvas->getView().getCenter().x) / 32;
    int view_y = static_cast<unsigned>(windowCanvas->getView().getCenter().y) / 32;
    for (unsigned i = 0; i < actors.size(); i++) {
        if (actors.at(i)->deactivate(view_x, view_y, 32)) {
            --i;
        }
    }
    gameEvents->activateEvents(windowCanvas->getView());

    // Run isAnimated tiles' timers
    gameTiles->advanceAnimatedTileTimers();
    // Run all actors' timers
    for (unsigned i = 0; i < actors.size(); i++) {
        actors.at(i)->advanceAnimationTimers();
        actors.at(i)->advanceTimers();
    }

    // Run all actors' tick events
    for (unsigned i = 0; i < actors.size(); i++) {
        actors.at(i)->tickEvent();
        if (actors.at(i)->perish()) {
            --i;
        }
    }

    processControlEvents();

    // Draw the layers: first lower (background and sprite) levels...
    gameTiles->drawLowerLevels();
    // ...then draw all the actors...
    for (unsigned i = 0; i < actors.size(); i++) {
        actors.at(i)->drawUpdate();
    }
    // ...then all the debris elements...
    for (unsigned i = 0; i < debris.size(); i++) {
        debris.at(i)->tickUpdate();
        if (debris.at(i)->getY() - windowCanvas->getView().getCenter().y > 400) {
            debris.erase(debris.begin() + i);
            --i;
        }
    }
    // ...and finally the higher (foreground) levels
    gameTiles->drawHigherLevels();

    // Draw the lighting overlay
    sf::Sprite s(lightTexture->getTexture());
    s.setColor(sf::Color(255, 255, 255, (255 * (100 - lightingLevel) / 100)));
    s.setOrigin(800, 600);
    s.setPosition(players[0]->getPosition().x, players[0]->getPosition().y - 15); // middle of the sprite vertically
    windowCanvas->draw(s);

    // Draw the UI
    windowCanvas->setView(*uiView);

    // Draw the character icon; managed by the player object
    players[0]->drawUIOverlay();

#ifdef CARROT_DEBUG
    if (dbgOverlaysActive) {
        //BitmapString::drawString(window,mainFont,"Frame: " + QString::number(frame),6,56);
        BitmapString::drawString(getCanvas(), getFont(), "Actors: " + QString::number(actors.size()), 6, 176);
        BitmapString::drawString(getCanvas(), getFont(), "FPS: " + QString::number(fps, 'f', 2) + " at " +
            QString::number(1000 / fps) + "ms/f", 6, 56);
        BitmapString::drawString(getCanvas(), getFont(), "Mod-" + QString::number(currentTempModifier) + " " +
            tempModifierName[currentTempModifier] + ": " + QString::number(tempModifier[currentTempModifier]), 6, 540);
    }
#endif

    // Update the drawn surface to the screen and set the view back to the player
    //windowCanvas->display();
    windowCanvas->updateContents();
    windowCanvas->setView(*gameView);
}

unsigned CarrotQt5::getLevelWidth() {
    // todo: phase out
    return gameTiles->getLevelWidth();
}
unsigned CarrotQt5::getLevelHeight() {
    // todo: phase out
    return gameTiles->getLevelHeight();
}

bool CarrotQt5::addActor(std::shared_ptr<CommonActor> actor) {
    actors.push_back(actor);
    return true;
}

bool CarrotQt5::addPlayer(std::shared_ptr<Player> actor, short playerID) {
    // If player ID is defined and is between 0 and 31 (inclusive),
    // try to add the player to the player array
    if ((playerID > -1) && (playerID < 32)) {
        if (players[playerID] != nullptr) {
            // A player with that number already existed
            return false;
        }
        // all OK, add to the player 
        players[playerID] = actor;
    }
    actors.push_back(actor);
    return true;
}

bool CarrotQt5::loadLevel(const QString& name) {
    QDir levelDir(name);
    if (levelDir.exists()) {
        QStringList levelFiles = levelDir.entryList(QStringList("*.layer") << "config.ini");
        if (levelFiles.contains("spr.layer") && levelFiles.contains("config.ini")) {
            // Required files found

            QSettings level_config(levelDir.absoluteFilePath("config.ini"),QSettings::IniFormat);

            if (level_config.value("Version/LayerFormat", 1).toUInt() > LAYERFORMATVERSION) {
                QMessageBox msg;
                msg.setWindowTitle("Error loading level");
                msg.setText("The level is using a too recent layer format. You might need to update to a newer game version.");
                msg.setStandardButtons(QMessageBox::Ok);
                msg.setDefaultButton(QMessageBox::Ok);
                msg.setIcon(QMessageBox::Critical);
                msg.exec();
                return false;
            }
            
            setLevelName(level_config.value("Level/FormalName", "Unnamed level").toString());

            // Clear the window contents
            windowCanvas->clear();

            // Show loading screen
            sf::Texture loadingScreenTexture;
            loadingScreenTexture.loadFromFile("Data/Assets/screen_loading.png");
            sf::Sprite loadingScreenSprite(loadingScreenTexture);
            loadingScreenSprite.setPosition(80, 60);
            windowCanvas->draw(loadingScreenSprite);
            BitmapString::drawString(getCanvas(), getFont(), levelName, 400, 360, FONT_ALIGN_CENTER);
            windowCanvas->updateContents();
            
            QString tileset = level_config.value("Level/Tileset", "").toString();
            
            nextLevel = level_config.value("Level/Next", "").toString();
                
            
            QDir tileset_dir(QDir::currentPath() + QString::fromStdString("/Tilesets/") + tileset);
            if (tileset_dir.exists()) {
                // Read the tileset and the sprite layer
                gameTiles = std::make_shared<TileMap>(shared_from_this(),
                                         tileset_dir.absoluteFilePath("tiles.png"),
                                         tileset_dir.absoluteFilePath("mask.png"),
                                         levelDir.absoluteFilePath("spr.layer"));
            
                // Read the sky layer if it exists
                if (levelFiles.contains("sky.layer")) {
                    gameTiles->readLayerConfiguration(LAYER_SKY_LAYER, levelDir.absoluteFilePath("sky.layer"), 0, level_config);
                }
            
                // Read the background layers
                QStringList bgLayers = levelFiles.filter(".bg.layer");
                for (unsigned i = 0; i < bgLayers.size(); ++i) {
                    gameTiles->readLayerConfiguration(LAYER_BACKGROUND_LAYER, levelDir.absoluteFilePath(bgLayers.at(i)), i, level_config);
                }

                // Read the foreground layers
                QStringList fgLayers = levelFiles.filter(".fg.layer");
                for (unsigned i = 0; i < fgLayers.size(); ++i) {
                    gameTiles->readLayerConfiguration(LAYER_FOREGROUND_LAYER, levelDir.absoluteFilePath(fgLayers.at(i)), i, level_config);
                }

                if (levelDir.entryList().contains("animtiles.dat")) {
                    gameTiles->readAnimatedTiles(levelDir.absoluteFilePath("animtiles.dat"));
                }
                
                gameEvents = std::make_shared<EventMap>(shared_from_this(), getLevelWidth(), getLevelHeight());
                if (levelFiles.contains("event.layer")) {
                    gameEvents->readEvents(levelDir.absoluteFilePath("event.layer"), level_config.value("Version/LayerFormat", 1).toUInt());
                }

                gameTiles->saveInitialSpriteLayer();
                
                if (players[0] == nullptr) {
                    auto defaultplayer = std::make_shared<Player>(shared_from_this(), 320.0, 32.0);
                    addPlayer(defaultplayer, 0);
                }
                
                resourceManager->getSoundSystem()->setMusic(("Music/" + level_config.value("Level/MusicDefault", "").toString().toUtf8()).data());
                setLighting(level_config.value("Level/LightInit", 100).toInt(),true);
                
                connect(ui.debug_health, SIGNAL(triggered()), players[0].get(), SLOT(debugHealth()));
                connect(ui.debug_ammo, SIGNAL(triggered()), players[0].get(), SLOT(debugAmmo()));

                setSavePoint();
            } else {
                QMessageBox msg;
                msg.setWindowTitle("Error loading level");
                msg.setText("Unknown tileset " + tileset);
                msg.setStandardButtons(QMessageBox::Ok);
                msg.setDefaultButton(QMessageBox::Ok);
                msg.setIcon(QMessageBox::Critical);
                msg.exec();
                return false;
            }
        } else {
            QMessageBox msg;
            msg.setWindowTitle("Error loading level");
            msg.setText("Sprite layer file (spr.layer) or configuration file (config.dat) missing!");
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setDefaultButton(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Critical);
            msg.exec();
            return false;
        }
    } else {
        QMessageBox msg;
        msg.setWindowTitle("Error loading level");
        msg.setText("Level folder " + name + " doesn't exist!");
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setDefaultButton(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.exec();
        return false;
    }
    return true;
}

void CarrotQt5::setLevelName(const QString& name) {
    levelName = name;
    setWindowTitle("Project Carrot - " + levelName);
}

void CarrotQt5::removeActor(std::shared_ptr<CommonActor> actor) {
    for (int i = 0; i < actors.size(); ++i) {
        if (actors.at(i) == actor) {
            actors.erase(actors.begin() + i);
            return;
        }
    }
}

QVector<std::weak_ptr<CommonActor>> CarrotQt5::findCollisionActors(CoordinatePair pos, std::shared_ptr<CommonActor> me) {
    QVector<std::weak_ptr<CommonActor>> res;
    for (int i = 0; i < actors.size(); ++i) {
        CoordinatePair pos2 = actors.at(i)->getPosition();
        if ((std::abs(pos.x - pos2.x) < 10) && (std::abs(pos.y - pos2.y) < 10)) {
            res << actors.at(i);
        }
    }
    return res;
}

QVector<std::weak_ptr<CommonActor>> CarrotQt5::findCollisionActors(Hitbox hitbox, std::shared_ptr<CommonActor> me) {
    QVector<std::weak_ptr<CommonActor>> res;
    for (int i = 0; i < actors.size(); ++i) {
        if (me == actors.at(i)) {
            continue;
        }
        if (actors.at(i)->getHitbox().overlaps(hitbox)) {
            res << actors.at(i);
        }
    }
    return res;
}

#ifdef CARROT_DEBUG
void CarrotQt5::debugShowMasks(bool show) {
    dbgShowMasked = show;
}

void CarrotQt5::debugLoadMusic() {
    QString filename = QFileDialog::getOpenFileName(this, "Load which file?", qApp->applicationDirPath(),
        "Module files (*.it *.s3m *.xm *.mod *.mo3 *.mtm *.umx);;All files (*.*)");
    if (filename.endsWith(".j2b")) {
        filename = JJ2Format::convertJ2B(filename);
    }
    if (filename != "") {
        resourceManager->getSoundSystem()->setMusic(filename);
    }
}

void CarrotQt5::debugSetGravity() {
    gravity = QInputDialog::getDouble(this, "Set new gravity", "Gravity:", gravity, -3.0, 3.0, 4);
}

void CarrotQt5::debugSetLighting() {
    lightingLevel = QInputDialog::getInt(this, "Set new lighting", "Lighting:", lightingLevel, 0, 100);
}

void CarrotQt5::debugSetPosition() {
    auto player = getPlayer(0).lock();
    if (player == nullptr) {
        return;
    }

    int x = QInputDialog::getInt(this, "Move player", "X position", player->getPosition().x, 0, getLevelWidth() * 32);
    int y = QInputDialog::getInt(this, "Move player", "Y position", player->getPosition().y, 0, getLevelHeight() * 32);
    player->moveInstantly({ x * 1.0, y * 1.0 });
}

void CarrotQt5::debugSetOverlaysActive(bool active) {
    dbgOverlaysActive = active;
}
#endif

void CarrotQt5::setSavePoint() {
    lastSavePoint.playerLives = players[0]->getLives() - 1;
    lastSavePoint.playerPosition = players[0]->getPosition();
    lastSavePoint.spriteLayerState = gameTiles->prepareSavePointLayer();
}

void CarrotQt5::loadSavePoint() {
    clearActors();
    gameEvents->deactivateAll();
    players[0]->moveInstantly(lastSavePoint.playerPosition);
    gameTiles->loadSavePointLayer(lastSavePoint.spriteLayerState);
}

void CarrotQt5::clearActors() {
    actors.clear();
    for (uint i = 0; i < 32; ++i) {
        if (players[i] != nullptr) {
            actors << players[i];
        }
    }
}

unsigned long CarrotQt5::getFrame() {
    return frame;
}

void CarrotQt5::createDebris(unsigned tileId, int x, int y) {
    for (int i = 0; i < 4; ++i) {
        auto d = std::make_shared<DestructibleDebris>(gameTiles->getTilesetTexture(), getCanvas(), x, y, tileId % 10, tileId / 10, i);
        debris << d;
    }
}

void CarrotQt5::setLighting(int target, bool immediate) {
    targetLightingLevel = target;
    if (target == lightingLevel) {
        return;
    }
    if (immediate) {
        lightingLevel = target;
    } else {
        // TODO: Bind to frames somehow
        QTimer::singleShot(50, this, SLOT(setLightingStep()));
    }
}

void CarrotQt5::setLightingStep() {
    if (targetLightingLevel == lightingLevel) {
        return;
    }
    short dir = (targetLightingLevel < lightingLevel) ? -1 : 1;
    lightingLevel += dir;

    // TODO: Bind to frames somehow
    QTimer::singleShot(50, this, SLOT(setLightingStep()));
}

void CarrotQt5::initLevelChange(ExitType e) {
    lastExit = e;
    resourceManager->getSoundSystem()->setMusic("");

    // TODO: Bind to frames somehow
    QTimer::singleShot(6000, this, SLOT(delayedLevelChange()));
}

void CarrotQt5::delayedLevelChange() {
    if (lastExit == NEXT_NORMAL) {
        LevelCarryOver o = players[0]->prepareLevelCarryOver();
        // TODO handle in a better way
        // The principal QTimer is likely to fire during this process.
        // Pausing should allow the tick function to be mostly ignored without ill effects.
        paused = true;

        cleanUpLevel();
        if (loadLevel("Levels/" + nextLevel)) {
            players[0]->receiveLevelCarryOver(o);
            lastExit = NEXT_NONE;
            paused = false;
        } else {
            startMainMenu();
        }
    }
}

bool CarrotQt5::isPositionEmpty(const Hitbox& hitbox, bool downwards, std::shared_ptr<CommonActor> me,
    std::weak_ptr<SolidObject>& collisionActor) {
    collisionActor = std::weak_ptr<SolidObject>();
    if (!gameTiles->isTileEmpty(hitbox, downwards)) {
        return false;
    }

    // check for solid objects
    QVector<std::weak_ptr<CommonActor>> collision = findCollisionActors(hitbox, me);
    for (int i = 0; i < collision.size(); ++i) {
        auto collisionPtr = collision.at(i).lock();
        if (collisionPtr == nullptr) {
            continue;
        }

        auto object = std::dynamic_pointer_cast<SolidObject>(collisionPtr);
        if (object == nullptr) {
            continue;
        }

        if (!object->getIsOneWay() || downwards) {
            collisionActor = object;
            return false;
        }
    }
    return true;
}

// alternate version to be used if we don'loadingScreenTexture care what solid object we collided with
bool CarrotQt5::isPositionEmpty(const Hitbox& hitbox, bool downwards, std::shared_ptr<CommonActor> me) {
    std::weak_ptr<SolidObject> placeholder;
    return isPositionEmpty(hitbox, downwards, me, placeholder);
}

QVector<std::weak_ptr<Player>> CarrotQt5::getCollidingPlayer(const Hitbox& hitbox) {
    QVector<std::weak_ptr<Player>> result;

    for (auto p : players) {
        if (p == nullptr) {
            continue;
        }

        Hitbox pHitbox = p->getHitbox();
        if (!(
            pHitbox.right < hitbox.left || pHitbox.left > hitbox.right ||
            pHitbox.bottom < hitbox.top || pHitbox.top > hitbox.bottom
        )) {
            result << p;
        }
    }

    return result;
}

unsigned CarrotQt5::getViewHeight() {
    return gameView->getSize().y;
}

CoordinatePair CarrotQt5::getViewCenter() {
    auto center = gameView->getCenter();
    return { center.x, center.y };
}

unsigned CarrotQt5::getViewWidth() {
    return gameView->getSize().x;
}

std::weak_ptr<Player> CarrotQt5::getPlayer(unsigned number) {
    if (number > 32) {
        return std::weak_ptr<Player>();
    } else {
        return players[number];
    }
}

std::weak_ptr<TileMap> CarrotQt5::getGameTiles() {
    return gameTiles;
}

std::weak_ptr<EventMap> CarrotQt5::getGameEvents() {
    return gameEvents;
}

std::shared_ptr<ResourceSet> CarrotQt5::loadActorTypeResources(const QString& actorType) {
    return resourceManager->loadActorTypeResources(actorType);
}

int CarrotQt5::getLightingLevel() {
    return lightingLevel;
}

void CarrotQt5::centerView(double x, double y) {
    gameView->setCenter(x, y);
}

void CarrotQt5::centerView(CoordinatePair pair) {
    gameView->setCenter(pair.x, pair.y);
}

void CarrotQt5::invokeFunction(InvokableRootFunction func, QVariant param) {
    (this->*func)(param);
}

void CarrotQt5::quitFromMainMenu(QVariant param) {
    this->close();
}

std::weak_ptr<CarrotCanvas> CarrotQt5::getCanvas() {
    return this->windowCanvas;
}

std::shared_ptr<BitmapFont> CarrotQt5::getFont() {
    return mainFont;
}

std::weak_ptr<SoundSystem> CarrotQt5::getSoundSystem() {
    return resourceManager->getSoundSystem();
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    std::shared_ptr<CarrotQt5> w = std::make_shared<CarrotQt5>();
    w->show();
    w->parseCommandLine();
    return a.exec();
}
