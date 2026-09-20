#include "app/AppBackend.h"
#include "app/ProdutoFotoProvider.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "services/backup/BackupService.h"
#include "services/log/LogService.h"

#include <QByteArray>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

// Sem console no pacote da loja, um "return 1" fechava o sistema sem dizer
// nada: o dono via o programa não abrir e não tinha por onde começar. Mostra o
// motivo numa janela simples e aponta o registro do sistema.
static int mostrarFalha(QGuiApplication &app, const QString &motivo)
{
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("motivoFalha"), motivo);
    engine.rootContext()->setContextProperty(QStringLiteral("pastaLog"), LogService::pasta());
    engine.loadData(QByteArrayLiteral(R"QML(
import QtQuick
import QtQuick.Controls
ApplicationWindow {
    visible: true
    width: 560; height: 300
    title: "Empório dos Amigos — não foi possível abrir"
    Column {
        anchors.fill: parent; anchors.margins: 24; spacing: 14
        Label { text: "O sistema não conseguiu abrir o banco de dados."; font.pixelSize: 18; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
        Label { text: motivoFalha; width: parent.width; wrapMode: Text.WordWrap }
        Label { text: "Nada foi apagado. Feche este aviso e tente abrir de novo. Se continuar, envie o arquivo sistema.log desta pasta para o suporte:
" + pastaLog; width: parent.width; wrapMode: Text.WordWrap }
        Button { text: "Fechar"; onClicked: Qt.quit() }
    }
}
)QML"));
    app.exec();
    return 1;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // Fontes da identidade (Archivo p/ UI, Fraunces p/ marca/títulos).
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-Variable.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Fraunces-Variable.ttf"));
    QGuiApplication::setFont(QFont(QStringLiteral("Archivo")));

    QGuiApplication::setApplicationName(QStringLiteral("Distribuidora"));
    QGuiApplication::setOrganizationName(QStringLiteral("Distribuidora"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // Registro em arquivo: o executável de produção não tem console, então sem
    // isto um erro ou fechamento inesperado não deixaria rastro na loja.
    // Instalado logo após o nome/versão do app (que entram na primeira linha).
    LogService::instalar();

    // --- Restauração pendente: troca o arquivo do banco ANTES de abri-lo ---
    // (agendada na tela de Backup; roda uma vez, no próximo início).
    {
        QString erroRestore;
        if (!BackupService::aplicarRestauracaoPendente(Database::defaultDatabasePath(),
                                                       &erroRestore)) {
            qWarning("Restauração pendente falhou: %s", qUtf8Printable(erroRestore));
        }
    }

    // --- Banco de dados: abre, aplica migrations e seed antes de subir a UI ---
    Database db;
    if (!db.open(Database::defaultDatabasePath())) {
        qCritical("Falha ao abrir o banco: %s", qUtf8Printable(db.lastError()));
        return mostrarFalha(app, db.lastError());
    }

    MigrationRunner runner(db.connection());
    if (!runner.migrate()) {
        qCritical("Falha ao migrar o banco: %s", qUtf8Printable(runner.lastError()));
        return mostrarFalha(app, runner.lastError());
    }
    if (!runner.appliedInLastRun().isEmpty()) {
        qInfo("Migrations aplicadas: %s",
              qUtf8Printable(runner.appliedInLastRun().join(QStringLiteral(", "))));
    }
    if (!runner.seed()) {
        qCritical("Falha ao carregar seed: %s", qUtf8Printable(runner.lastError()));
        return mostrarFalha(app, runner.lastError());
    }

    // --- Fachada de negócio exposta ao QML ---
    AppBackend backend(db.connection());

    // --- Interface QML ---
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &backend);
    // Fotos dos produtos vêm do banco: image://produto/<id>
    engine.addImageProvider(QString::fromLatin1(ProdutoFotoProvider::nome()),
                            new ProdutoFotoProvider(db.connection()));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("Distribuidora", "Main");

    return app.exec();
}
