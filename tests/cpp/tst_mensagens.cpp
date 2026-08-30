#include <QtTest>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"

// A mensagem de erro é interface. Quando o sistema despeja o texto cru do banco
// ("UNIQUE constraint failed: produto_embalagens.codigo_barras Unable to fetch
// row"), quem está no balcão não entende, não sabe o que fazer e liga para o
// dono — que também não entende. Este teste trava as frases nos erros que mais
// acontecem no dia a dia.
//
// Trava também o caso do valor escrito errado: antes, texto que não era número
// virava R$ 0,00 em silêncio na abertura do caixa.
class TstMensagens : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void codigoDeBarrasRepetidoExplicaOQueFazer();
    void erroTecnicoNuncaChegaCruNaTela();
    void aberturaDeCaixaRecusaValorIlegivel();
    void fechamentoDeCaixaRecusaValorIlegivel();
    void mensagemDoProprioSistemaPassaIntacta();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    int m_categoriaId = 0;

    bool criarProduto(const QString &nome, const QString &codigo);
};

void TstMensagens::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));
    m_categoriaId = m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();
}

bool TstMensagens::criarProduto(const QString &nome, const QString &codigo)
{
    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = nome;
    p[QStringLiteral("categoriaId")] = m_categoriaId;
    QVariantMap e;
    e[QStringLiteral("id")] = 0;
    e[QStringLiteral("nome")] = QStringLiteral("Unidade");
    e[QStringLiteral("fator")] = 1;
    e[QStringLiteral("codigoBarras")] = codigo;
    e[QStringLiteral("preco")] = 500;
    e[QStringLiteral("custo")] = -1;
    p[QStringLiteral("embalagens")] = QVariantList{e};
    return m_app->salvarProduto(p);
}

// O erro mais comum do cadastro: bipar um código que já é de outro produto.
void TstMensagens::codigoDeBarrasRepetidoExplicaOQueFazer()
{
    QVERIFY(criarProduto(QStringLiteral("Coca 2L"), QStringLiteral("7891000")));
    QVERIFY2(!criarProduto(QStringLiteral("Guaraná 2L"), QStringLiteral("7891000")),
             "código repetido tem que ser recusado");

    const QString msg = m_app->ultimoErro();
    QVERIFY2(msg.contains(QStringLiteral("código de barras"), Qt::CaseInsensitive),
             qUtf8Printable(msg));
    QVERIFY2(msg.contains(QStringLiteral("outro produto")), qUtf8Printable(msg));
}

// Nenhuma mensagem mostrada ao operador pode conter jargão do banco.
void TstMensagens::erroTecnicoNuncaChegaCruNaTela()
{
    QVERIFY(!criarProduto(QStringLiteral("Outro"), QStringLiteral("7891000")));
    const QString msg = m_app->ultimoErro();

    const QStringList jargao = {QStringLiteral("constraint"), QStringLiteral("UNIQUE"),
                                QStringLiteral("FOREIGN KEY"), QStringLiteral("NOT NULL"),
                                QStringLiteral("Unable to"), QStringLiteral("sqlite"),
                                QStringLiteral("SQL")};
    for (const QString &j : jargao) {
        QVERIFY2(!msg.contains(j, Qt::CaseSensitive),
                 qUtf8Printable(QStringLiteral("mensagem contém '%1': %2").arg(j, msg)));
    }
}

// "1OO" com a letra O abria o caixa com R$ 0,00 sem avisar. No fechamento
// aparecia uma diferença de 100 reais que ninguém sabia explicar.
void TstMensagens::aberturaDeCaixaRecusaValorIlegivel()
{
    QVERIFY2(!m_app->abrirCaixa(QStringLiteral("1OO")), "texto ilegível não pode abrir o caixa");
    QVERIFY(!m_app->caixaAberto());
    QVERIFY(m_app->ultimoErro().contains(QStringLiteral("inválido"), Qt::CaseInsensitive));

    // Vazio continua valendo: abrir sem troco na gaveta é legítimo.
    QVERIFY2(m_app->abrirCaixa(QString()), qUtf8Printable(m_app->ultimoErro()));
    QVERIFY(m_app->caixaAberto());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("abertura")).toLongLong(), qlonglong(0));
}

// Fechamento é a operação mais sensível: valor ilegível não pode virar "contei
// zero" por omissão, nem quando alguém chama o backend direto.
void TstMensagens::fechamentoDeCaixaRecusaValorIlegivel()
{
    QVERIFY(m_app->caixaAberto());
    const QVariantMap r = m_app->fecharCaixa(QStringLiteral("duzentos"));
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(!r.value(QStringLiteral("erro")).toString().isEmpty());
    QVERIFY2(m_app->caixaAberto(), "o caixa não pode ter fechado");
}

// A tradução não pode estragar as mensagens que o próprio sistema escreve.
void TstMensagens::mensagemDoProprioSistemaPassaIntacta()
{
    QVariantMap u = m_app->novoUsuario();
    u[QStringLiteral("nome")] = QStringLiteral("Outro");
    u[QStringLiteral("login")] = QStringLiteral("dono");
    u[QStringLiteral("perfilId")] = 2;
    QVERIFY(!m_app->salvarUsuario(u, QStringLiteral("senha12345")));
    QCOMPARE(m_app->ultimoErro(), QStringLiteral("Já existe um usuário com esse login."));
}

QTEST_MAIN(TstMensagens)
#include "tst_mensagens.moc"
