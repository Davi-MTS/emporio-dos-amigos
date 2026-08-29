// =============================================================================
// tests/qml — arnês (harness) dos testes de interface
// =============================================================================
// Os 16 testes de C++ cobrem regra de negócio; nenhum abria uma tela. Todos os
// defeitos visuais encontrados em uso (diálogo que não fechava, campo cortado,
// coluna sobrepondo produto, lista perdendo os insumos) passavam ilesos por
// eles. Este arnês sobe as MESMAS telas do app, com um AppBackend de verdade
// sobre um banco temporário, para que os testes .qml possam instanciá-las.
//
// O banco vive num QTemporaryDir e QStandardPaths fica em modo de teste: nada
// aqui toca no banco, nos backups ou nas configurações da loja.
// =============================================================================

#include <QtQuickTest>

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"

class ArnesTelas : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void applicationAvailable()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DistribuidoraTeste"));
        QCoreApplication::setApplicationName(QStringLiteral("DistribuidoraTeste"));
        QQuickStyle::setStyle(QStringLiteral("Fusion"));

        m_pasta.reset(new QTemporaryDir);
        Q_ASSERT(m_pasta->isValid());

        m_db.reset(new Database);
        const bool abriu = m_db->open(m_pasta->filePath(QStringLiteral("teste.db")));
        Q_ASSERT(abriu);
        Q_UNUSED(abriu)

        MigrationRunner runner(m_db->connection());
        const bool migrou = runner.migrate() && runner.seed();
        Q_ASSERT(migrou);
        Q_UNUSED(migrou)

        m_backend.reset(new AppBackend(m_db->connection()));

        // As telas reais só aparecem com alguém logado (permissões). O admin é
        // criado pelo próprio app no primeiro boot, então fazemos o mesmo aqui.
        m_backend->criarAdmin(QStringLiteral("Teste"),
                              QStringLiteral("teste"),
                              QStringLiteral("teste1234"));
        m_backend->login(QStringLiteral("teste"), QStringLiteral("teste1234"));
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // Mesmo nome usado em main.cpp: as telas chamam App.* sem saber que
        // estão num teste.
        engine->rootContext()->setContextProperty(QStringLiteral("App"), m_backend.data());
    }

    void cleanupTestCase()
    {
        m_backend.reset();
        m_db.reset();
        m_pasta.reset();
    }

private:
    QScopedPointer<QTemporaryDir> m_pasta;
    QScopedPointer<Database> m_db;
    QScopedPointer<AppBackend> m_backend;
};

QUICK_TEST_MAIN_WITH_SETUP(telas, ArnesTelas)

#include "arnes_qml.moc"
