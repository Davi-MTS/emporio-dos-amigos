#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "services/backup/BackupService.h"
#include "services/log/LogService.h"

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

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
        return 1;
    }

    MigrationRunner runner(db.connection());
    if (!runner.migrate()) {
        qCritical("Falha ao migrar o banco: %s", qUtf8Printable(runner.lastError()));
        return 2;
    }
    if (!runner.appliedInLastRun().isEmpty()) {
        qInfo("Migrations aplicadas: %s",
              qUtf8Printable(runner.appliedInLastRun().join(QStringLiteral(", "))));
    }
    if (!runner.seed()) {
        qCritical("Falha ao carregar seed: %s", qUtf8Printable(runner.lastError()));
        return 3;
    }

    // --- Fachada de negócio exposta ao QML ---
    AppBackend backend(db.connection());

    // --- Interface QML ---
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &backend);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("Distribuidora", "Main");

    return app.exec();
}
