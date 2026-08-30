#include <QtTest>

#include <QImage>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"

// A foto do produto mora DENTRO do banco, e o banco inteiro viaja no backup do
// Telegram (limite de 45 MB por arquivo). Se a imagem entrar no tamanho
// original, meia dúzia de fotos de celular (3 a 5 MB cada) estouram o envio e o
// backup para de sair da loja — o pior tipo de falha, porque é silenciosa.
//
// Este teste trava a redução.
class TstFotoProduto : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void fotoGrandeEReduzidaAoGravar();
    void removerApagaAFoto();
    void arquivoInvalidoNaoQuebra();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    int m_produtoId = 0;

    ProdutoRepository prod() { return ProdutoRepository(m_db.connection()); }
    QString criarImagem(const QString &nome, int largura, int altura);
};

void TstFotoProduto::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = QStringLiteral("Produto com foto");
    p[QStringLiteral("categoriaId")] =
        m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();
    QVERIFY2(m_app->salvarProduto(p), qUtf8Printable(m_app->ultimoErro()));
    for (const Produto &pr : prod().listar(QStringLiteral("Produto com foto")))
        m_produtoId = pr.id;
    QVERIFY(m_produtoId > 0);
}

QString TstFotoProduto::criarImagem(const QString &nome, int largura, int altura)
{
    // Imagem com ruído: um PNG de cor sólida comprimiria a quase nada e o teste
    // passaria sem provar que houve redução.
    QImage img(largura, altura, QImage::Format_RGB32);
    for (int y = 0; y < altura; ++y) {
        for (int x = 0; x < largura; ++x)
            img.setPixel(x, y, qRgb((x * 7 + y * 13) % 256, (x * 3) % 256, (y * 5) % 256));
    }
    const QString caminho = m_dir.filePath(nome);
    if (!img.save(caminho, "PNG"))
        return {};
    return caminho;
}

void TstFotoProduto::fotoGrandeEReduzidaAoGravar()
{
    const QString caminho = criarImagem(QStringLiteral("grande.png"), 2000, 1500);
    QVERIFY(!caminho.isEmpty());

    const QVariantMap r = m_app->definirFotoProduto(m_produtoId, caminho);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    const QByteArray guardada = prod().foto(m_produtoId);
    QVERIFY(!guardada.isEmpty());

    // Nada de comparar com o tamanho do ARQUIVO de origem: um PNG sintético
    // comprime muito bem e a comparação mediria o gerador do teste, não o
    // sistema. O que interessa é o resultado absoluto.

    // 1) O lado maior foi para 320 px — prova que houve redução.
    QImage lida;
    QVERIFY(lida.loadFromData(guardada, "JPEG"));
    QCOMPARE(qMax(lida.width(), lida.height()), 320);
    QCOMPARE(lida.width() > lida.height(), true);   // manteve a proporção
    QCOMPARE(lida.height(), 240);                   // 2000x1500 -> 320x240

    // 2) Cabe no orçamento do backup: com 100 KB por foto, mil produtos já
    //    passariam dos 45 MB que o Telegram aceita por arquivo.
    QVERIFY2(guardada.size() < 100 * 1024,
             qPrintable(QStringLiteral("foto guardada com %1 B").arg(guardada.size())));

    QVERIFY(m_app->produtoTemFoto(m_produtoId));
}

void TstFotoProduto::removerApagaAFoto()
{
    QVERIFY(m_app->produtoTemFoto(m_produtoId));
    QVERIFY(m_app->removerFotoProduto(m_produtoId));
    QVERIFY(!m_app->produtoTemFoto(m_produtoId));
    QVERIFY(prod().foto(m_produtoId).isEmpty());
}

// Escolher um arquivo que não é imagem tem que avisar, não travar.
void TstFotoProduto::arquivoInvalidoNaoQuebra()
{
    const QString txt = m_dir.filePath(QStringLiteral("nao-e-imagem.txt"));
    {
        QFile f(txt);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("isto aqui e um texto qualquer");
    }
    const QVariantMap r = m_app->definirFotoProduto(m_produtoId, txt);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(!r.value(QStringLiteral("erro")).toString().isEmpty());
    QVERIFY(!m_app->produtoTemFoto(m_produtoId));
}

QTEST_MAIN(TstFotoProduto)
#include "tst_foto_produto.moc"
