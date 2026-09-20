#include <QtTest>

#include <QAtomicInt>
#include <QImage>
#include <QTemporaryDir>
#include <QThread>

#include "app/AppBackend.h"
#include "app/ProdutoFotoProvider.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"

// O sistema FECHAVA INTEIRO ao pôr foto num produto, na loja.
//
// A miniatura usa `asynchronous: true`, e com isso o Qt chama
// ProdutoFotoProvider::requestImage numa THREAD SEPARADA (está na documentação
// do QQuickImageProvider). O provider usava a mesma conexão de banco da thread
// principal — e uma conexão do Qt SQL só pode ser usada pela thread que a
// criou. Ao gravar a foto, a lista de produtos recarrega na thread principal ao
// mesmo tempo em que as miniaturas são pedidas na outra: as duas mexem juntas na
// mesma conexão e o programa cai.
//
// Só apareceu depois que `temFoto` passou a funcionar nas listas: antes, a lista
// nunca pedia imagem nenhuma, então só existia uma miniatura (a do editor) e a
// corrida quase nunca acontecia.
//
// Este teste reproduz exatamente isso: threads pedindo miniaturas enquanto a
// thread principal grava fotos e recarrega listas.
class TstFotoConcorrencia : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void miniaturasEmOutraThreadEnquantoGrava();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    QVector<int> m_ids;
    QString m_png;
};

void TstFotoConcorrencia::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    QImage img(640, 480, QImage::Format_RGB32);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            img.setPixel(x, y, qRgb((x * 5) % 256, (y * 3) % 256, (x + y) % 256));
    m_png = m_dir.filePath(QStringLiteral("foto.png"));
    QVERIFY(img.save(m_png, "PNG"));

    // Uma lista de produtos com foto, como a da loja depois de usar a fila.
    const int categoria = m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();
    ProdutoRepository repo(m_db.connection());
    for (int i = 0; i < 12; ++i) {
        QVariantMap p = m_app->novoProduto();
        const QString nome = QStringLiteral("Produto concorrente %1").arg(i);
        p[QStringLiteral("nome")] = nome;
        p[QStringLiteral("categoriaId")] = categoria;
        QVERIFY2(m_app->salvarProduto(p), qUtf8Printable(m_app->ultimoErro()));
        for (const Produto &pr : repo.listar(nome))
            if (pr.nome == nome)
                m_ids.push_back(pr.id);
    }
    QCOMPARE(m_ids.size(), 12);
    for (int id : std::as_const(m_ids)) {
        const QVariantMap r = m_app->definirFotoProduto(id, m_png);
        QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
                 qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    }
}

void TstFotoConcorrencia::miniaturasEmOutraThreadEnquantoGrava()
{
    ProdutoFotoProvider provider(m_db.connection());

    QAtomicInt parar(0);
    QAtomicInt pedidas(0);
    QAtomicInt vazias(0);

    // O Qt usa uma thread de imagens por engine; aqui são duas, para a corrida
    // aparecer em poucos segundos em vez de depender de sorte.
    const auto trabalho = [&]() {
        while (!parar.loadRelaxed()) {
            for (int id : std::as_const(m_ids)) {
                QSize tamanho;
                const QImage img = provider.requestImage(
                    QStringLiteral("%1?v=%2").arg(id).arg(pedidas.loadRelaxed()),
                    &tamanho, QSize(72, 72));
                pedidas.fetchAndAddRelaxed(1);
                if (img.isNull())
                    vazias.fetchAndAddRelaxed(1);
            }
        }
    };
    QScopedPointer<QThread> t1(QThread::create(trabalho));
    QScopedPointer<QThread> t2(QThread::create(trabalho));
    t1->start();
    t2->start();

    // Thread principal: o que acontece ao atribuir foto — grava, recarrega a
    // lista, recarrega o estoque, reconta quem está sem foto.
    QElapsedTimer relogio;
    relogio.start();
    int rodadas = 0;
    while (relogio.elapsed() < 4000) {
        const int id = m_ids.at(rodadas % m_ids.size());
        const QVariantMap r = m_app->definirFotoProduto(id, m_png);
        QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
                 qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
        m_app->recarregarEstoque();
        m_app->contarProdutosSemFoto();
        ++rodadas;
    }

    parar.storeRelaxed(1);
    QVERIFY(t1->wait(10000));
    QVERIFY(t2->wait(10000));

    qInfo("%d gravacoes na thread principal, %d miniaturas pedidas nas outras, %d vazias",
          rodadas, pedidas.loadRelaxed(), vazias.loadRelaxed());

    QVERIFY(rodadas > 10);
    QVERIFY(pedidas.loadRelaxed() > 100);
    // Todo produto aqui TEM foto: uma miniatura vazia é leitura que falhou por
    // causa da concorrência, não "produto sem foto".
    QCOMPARE(vazias.loadRelaxed(), 0);
}

QTEST_MAIN(TstFotoConcorrencia)
#include "tst_foto_concorrencia.moc"
