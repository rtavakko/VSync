#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent, QScreen *outputScreen,
                       OpenGLRenderer::OpenGLRenderSpecs specs,
                       unsigned int numDisplayWindows) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    videoSpecs(specs),
    mainOutputScreen(outputScreen),
    textureRenderer(nullptr),
    renderThread(nullptr),
    numDisplays(numDisplayWindows),
    textRefreshTime(150)
{
    ui->setupUi(this);

    bool initialized = initialize();
    assert(initialized);
}

MainWindow::~MainWindow()
{
    renderThread->quit();

    delete ui;
}

bool MainWindow::initialize()
{
    //Renderer
    renderThread = new QThread();
    textureRenderer = new OpenGLRenderSurface(mainOutputScreen,
                                              nullptr,
                                              videoSpecs,
                                              QSurfaceFormat::defaultFormat(),
                                              nullptr);

    textureRenderer->moveToThread(renderThread);

    QObject::connect(this,&MainWindow::setRenderFPS,textureRenderer,&OpenGLRenderSurface::setFrameRate);
    QObject::connect(ui->pbStartVideo,&QPushButton::released,textureRenderer,&OpenGLRenderSurface::start);
    QObject::connect(ui->pbStopVideo,&QPushButton::released,textureRenderer,&OpenGLRenderSurface::stop);

    ui->textFPS->setInputMask(QString("999"));                       //Allow numbers as input
    ui->textFPS->setText(QString::number(videoSpecs.frameRate));

    QObject::connect(ui->pbSetRenderFPS,&QPushButton::released,this,[=]()
    {
        emit setRenderFPS(ui->textFPS->text().toDouble());
    });
    QObject::connect(textureRenderer,&OpenGLRenderSurface::renderedFrame,this,[=](double actualFPS)
    {
        t_endRender = std::chrono::high_resolution_clock::now();
        t_deltaRender = t_endRender - t_startRender;

        if(t_deltaRender.count() >= textRefreshTime)
        {
            t_startRender = t_endRender;
            ui->lActualRenderFPS->setText(QString("Actual Render FPS: %1").arg(static_cast<unsigned int>(actualFPS)));
        }
    });

    QObject::connect(renderThread,&QThread::started,textureRenderer,&OpenGLRenderSurface::start);
    QObject::connect(renderThread,&QThread::finished,this,[=]()
    {
        delete textureRenderer;
        textureRenderer = nullptr;

        foreach(OpenGLDisplay* display, textureDisplay)
        {
            delete display;
            display = nullptr;
        }
    });

    //Displays
    textureDisplay.assign(numDisplays,nullptr);
    foreach(OpenGLDisplay* display, textureDisplay)
    {
        display = new OpenGLDisplay(mainOutputScreen,
                                    videoSpecs,
                                    QSurfaceFormat::defaultFormat(),
                                    textureRenderer->getOpenGLContext());
        QObject::connect(textureRenderer,&OpenGLRenderSurface::frameReady,display,&OpenGLDisplay::setFrame);
    }

    renderThread->start();

    return true;
}
