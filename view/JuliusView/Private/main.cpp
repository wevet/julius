#include <QApplication>
#include "Public/JuliusAnalyzeWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    JuliusAnalyzeWindow w;
    w.show();
    return app.exec();
}


