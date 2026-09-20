#include "app/ProdutoFotoProvider.h"

#include <QImage>
#include <QSqlQuery>
#include <QThread>
#include <QThreadStorage>

namespace {

// Conexão de LEITURA que pertence a uma única thread. O QThreadStorage apaga o
// objeto quando a thread termina, e o destrutor fecha e remove a conexão — sem
// isso o Qt reclamaria de conexão órfã ao fechar o programa.
class ConexaoDaThread
{
public:
    explicit ConexaoDaThread(const QString &caminho)
        : m_nome(QStringLiteral("fotos-thread-%1")
                     .arg(reinterpret_cast<quintptr>(QThread::currentThreadId())))
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_nome);
        db.setDatabaseName(caminho);
        // Só lê. O busy timeout cobre o instante em que a thread principal está
        // gravando a própria foto que vai ser pedida em seguida.
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=2000"));
        db.open();
    }

    ~ConexaoDaThread()
    {
        {
            QSqlDatabase db = QSqlDatabase::database(m_nome, false);
            if (db.isValid())
                db.close();
        }   // a cópia acima precisa morrer antes do removeDatabase
        QSqlDatabase::removeDatabase(m_nome);
    }

    QSqlDatabase banco() const { return QSqlDatabase::database(m_nome, false); }

private:
    const QString m_nome;
};

QThreadStorage<ConexaoDaThread *> g_conexoes;

}  // namespace

ProdutoFotoProvider::ProdutoFotoProvider(const QSqlDatabase &db)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_caminhoBanco(db.databaseName())
{
}

QByteArray ProdutoFotoProvider::lerFoto(int produtoId)
{
    if (!g_conexoes.hasLocalData())
        g_conexoes.setLocalData(new ConexaoDaThread(m_caminhoBanco));

    const QSqlDatabase db = g_conexoes.localData()->banco();
    if (!db.isOpen())
        return {};

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT foto FROM produtos WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), produtoId);
    if (!q.exec() || !q.next())
        return {};
    return q.value(0).toByteArray();
}

QImage ProdutoFotoProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "12?v=3" -> 12. A parte do "?" só serve para furar o cache do Qt.
    const QString soId = id.section(QLatin1Char('?'), 0, 0);
    bool ok = false;
    const int produtoId = soId.toInt(&ok);
    if (!ok || produtoId <= 0)
        return {};

    const QByteArray dados = lerFoto(produtoId);
    if (dados.isEmpty())
        return {};   // sem foto: a tela mostra o espaço vazio, não um erro

    QImage img;
    if (!img.loadFromData(dados, "JPEG"))
        return {};

    if (size)
        *size = img.size();

    // A foto já é pequena (320 px), mas se a tela pedir menor, entrega menor:
    // uma miniatura de 40 px numa lista longa não precisa carregar 320.
    if (requestedSize.isValid() && !requestedSize.isEmpty()
        && (requestedSize.width() < img.width() || requestedSize.height() < img.height())) {
        return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}
