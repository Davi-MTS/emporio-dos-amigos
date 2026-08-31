#include <QtTest>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"

// Copão com composição PADRÃO e preço que se ajusta na troca.
//
// A loja tem um copão por destilado. O de Beafeeter vem com 80 ml da bebida,
// um Extra Power, um gelo e um copo — e custa R$ 30,00 assim. Se o cliente
// pedir com Monster, que é R$ 5,00 mais caro na prateleira, o copão vai a
// R$ 35,00 sozinho, sem ninguém fazer conta no balcão.
//
// O ponto delicado é a ESCALA: não dá para comparar o preço das embalagens
// direto, senão trocar o destilado somaria uma garrafa inteira no copão em vez
// dos 80 ml que ele usa. A conta é por unidade base × quantidade da receita.
class TstCopao : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void composicaoPadraoSaiPeloPrecoCadastrado();
    void trocarPorMaisCaroSobeAPrecoDaDiferenca();
    void trocarPorMaisBaratoDesceOPreco();
    void trocaDeDestiladoUsaSoOQueAReceitaConsome();
    void semPadraoDefinidoNaoAjustaNada();
    void funcionarioNaoPodeMudarOPrecoDoCopao();
    void vendaCanceladaNaoDeixaCustoNoLucro();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;

    int m_copao = 0;
    int m_beafeeter = 0, m_ginCaro = 0;
    int m_extraPower = 0, m_monster = 0, m_energBarato = 0;
    int m_gelo = 0, m_copo = 0;
    int m_catDestilados = 0, m_catEnergetico = 0, m_catGelo = 0, m_catDescartaveis = 0;

    ProdutoRepository prod() { return ProdutoRepository(m_db.connection()); }
    int criar(const QString &nome, int categoria, const QString &unidadeBase,
              const QString &embalagem, int fator, qint64 preco);
    int categoria(const QString &nome);
};

int TstCopao::categoria(const QString &nome)
{
    for (const QVariant &v : m_app->categorias()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("nome")).toString() == nome)
            return m.value(QStringLiteral("id")).toInt();
    }
    return m_app->criarCategoria(nome);
}

int TstCopao::criar(const QString &nome, int categoriaId, const QString &unidadeBase,
                    const QString &embalagem, int fator, qint64 preco)
{
    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = nome;
    p[QStringLiteral("categoriaId")] = categoriaId;
    p[QStringLiteral("unidadeBase")] = unidadeBase;
    QVariantMap e;
    e[QStringLiteral("id")] = 0;
    e[QStringLiteral("nome")] = embalagem;
    e[QStringLiteral("fator")] = fator;
    e[QStringLiteral("codigoBarras")] = QString();
    e[QStringLiteral("preco")] = static_cast<qlonglong>(preco);
    e[QStringLiteral("custo")] = -1;
    p[QStringLiteral("embalagens")] = QVariantList{e};
    if (!m_app->salvarProduto(p))
        return 0;
    for (const Produto &pr : prod().listar(nome)) {
        if (pr.nome == nome)
            return pr.id;
    }
    return 0;
}

void TstCopao::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    m_catDestilados = categoria(QStringLiteral("Destilados"));
    m_catEnergetico = categoria(QStringLiteral("Energético"));
    m_catGelo = categoria(QStringLiteral("Gelo"));
    m_catDescartaveis = categoria(QStringLiteral("Descartáveis"));

    // Destilados em ml: garrafa de 750 ml. R$ 120,00 = 16 centavos por ml.
    m_beafeeter = criar(QStringLiteral("Beafeeter"), m_catDestilados,
                        QStringLiteral("ml"), QStringLiteral("Garrafa"), 750, 12000);
    // Gin mais caro: R$ 150,00 na mesma garrafa = 20 centavos por ml.
    m_ginCaro = criar(QStringLiteral("Gin Importado"), m_catDestilados,
                      QStringLiteral("ml"), QStringLiteral("Garrafa"), 750, 15000);

    m_extraPower = criar(QStringLiteral("Extra Power"), m_catEnergetico,
                         QStringLiteral("unidade"), QStringLiteral("Lata"), 1, 550);
    m_monster = criar(QStringLiteral("Monster"), m_catEnergetico,
                      QStringLiteral("unidade"), QStringLiteral("Lata"), 1, 1050);
    m_energBarato = criar(QStringLiteral("Energetico Simples"), m_catEnergetico,
                          QStringLiteral("unidade"), QStringLiteral("Lata"), 1, 350);

    m_gelo = criar(QStringLiteral("Gelo de coco"), m_catGelo,
                   QStringLiteral("unidade"), QStringLiteral("Pacote"), 1, 400);
    m_copo = criar(QStringLiteral("Copo descartavel"), m_catDescartaveis,
                   QStringLiteral("unidade"), QStringLiteral("Unidade"), 1, 20);

    QVERIFY(m_beafeeter > 0 && m_ginCaro > 0 && m_extraPower > 0 && m_monster > 0
            && m_energBarato > 0 && m_gelo > 0 && m_copo > 0);

    // O copão: R$ 30,00 com a composição padrão.
    QVariantMap c = m_app->novoProduto();
    c[QStringLiteral("nome")] = QStringLiteral("Copao de Beafeeter");
    c[QStringLiteral("categoriaId")] = m_catDestilados;
    c[QStringLiteral("composto")] = true;

    auto linha = [](int cat, const QString &un, int qtd, int padrao, bool travada) {
        QVariantMap l;
        l[QStringLiteral("categoriaId")] = cat;
        l[QStringLiteral("unidade")] = un;
        l[QStringLiteral("quantidade")] = qtd;
        l[QStringLiteral("produtoPadraoId")] = padrao;
        l[QStringLiteral("travada")] = travada;
        return l;
    };
    // Destilados vem TRAVADO: existe um copão por bebida.
    c[QStringLiteral("composicao")] = QVariantList{
        linha(m_catDestilados, QStringLiteral("ml"), 80, m_beafeeter, true),
        linha(m_catEnergetico, QStringLiteral("unidade"), 1, m_extraPower, false),
        linha(m_catGelo, QStringLiteral("unidade"), 1, m_gelo, false),
        linha(m_catDescartaveis, QStringLiteral("unidade"), 1, m_copo, false)};

    QVariantMap emb;
    emb[QStringLiteral("id")] = 0;
    emb[QStringLiteral("nome")] = QStringLiteral("Copao");
    emb[QStringLiteral("fator")] = 1;
    emb[QStringLiteral("preco")] = 3000;   // R$ 30,00
    emb[QStringLiteral("custo")] = -1;
    c[QStringLiteral("embalagens")] = QVariantList{emb};
    QVERIFY2(m_app->salvarProduto(c), qUtf8Printable(m_app->ultimoErro()));

    for (const Produto &pr : prod().listar(QStringLiteral("Copao de Beafeeter")))
        m_copao = pr.id;
    QVERIFY(m_copao > 0);
}

// Sem trocar nada, sai pelo preço cadastrado.
void TstCopao::composicaoPadraoSaiPeloPrecoCadastrado()
{
    const QVariantList linhas = m_app->composicaoParaVenda(m_copao);
    QCOMPARE(linhas.size(), 4);

    QVariantList escolhas;
    for (const QVariant &v : linhas)
        escolhas.push_back(v.toMap().value(QStringLiteral("produtoPadraoId")));

    QCOMPARE(m_app->precoCompostoMontado(m_copao, escolhas), qlonglong(3000));

    // A linha do destilado tem que vir marcada como travada.
    bool achouTravada = false;
    for (const QVariant &v : linhas) {
        const QVariantMap l = v.toMap();
        if (l.value(QStringLiteral("categoriaId")).toInt() == m_catDestilados) {
            QCOMPARE(l.value(QStringLiteral("travada")).toBool(), true);
            QCOMPARE(l.value(QStringLiteral("produtoPadraoId")).toInt(), m_beafeeter);
            achouTravada = true;
        }
    }
    QVERIFY(achouTravada);
}

// O caso que a loja descreveu: Monster no lugar do Extra Power.
void TstCopao::trocarPorMaisCaroSobeAPrecoDaDiferenca()
{
    const QVariantList linhas = m_app->composicaoParaVenda(m_copao);
    QVariantList escolhas;
    for (const QVariant &v : linhas) {
        const QVariantMap l = v.toMap();
        escolhas.push_back(l.value(QStringLiteral("categoriaId")).toInt() == m_catEnergetico
                               ? m_monster
                               : l.value(QStringLiteral("produtoPadraoId")).toInt());
    }
    // 10,50 - 5,50 = 5,00 -> 30,00 + 5,00 = 35,00
    QCOMPARE(m_app->precoCompostoMontado(m_copao, escolhas), qlonglong(3500));

    // E a própria lista de opções já traz a diferença pronta para a tela.
    for (const QVariant &v : linhas) {
        const QVariantMap l = v.toMap();
        if (l.value(QStringLiteral("categoriaId")).toInt() != m_catEnergetico)
            continue;
        for (const QVariant &pv : l.value(QStringLiteral("produtos")).toList()) {
            const QVariantMap pm = pv.toMap();
            if (pm.value(QStringLiteral("id")).toInt() == m_monster)
                QCOMPARE(pm.value(QStringLiteral("diferenca")).toLongLong(), qlonglong(500));
            if (pm.value(QStringLiteral("id")).toInt() == m_extraPower)
                QCOMPARE(pm.value(QStringLiteral("diferenca")).toLongLong(), qlonglong(0));
        }
    }
}

// Simétrico: item mais barato reduz o copão na mesma proporção.
void TstCopao::trocarPorMaisBaratoDesceOPreco()
{
    const QVariantList linhas = m_app->composicaoParaVenda(m_copao);
    QVariantList escolhas;
    for (const QVariant &v : linhas) {
        const QVariantMap l = v.toMap();
        escolhas.push_back(l.value(QStringLiteral("categoriaId")).toInt() == m_catEnergetico
                               ? m_energBarato
                               : l.value(QStringLiteral("produtoPadraoId")).toInt());
    }
    // 3,50 - 5,50 = -2,00 -> 30,00 - 2,00 = 28,00
    QCOMPARE(m_app->precoCompostoMontado(m_copao, escolhas), qlonglong(2800));
}

// O ponto delicado: a receita usa 80 ml, não a garrafa inteira. A diferença
// entre as garrafas é de R$ 30,00; no copão tem que entrar só o pedaço usado.
void TstCopao::trocaDeDestiladoUsaSoOQueAReceitaConsome()
{
    const QVariantList linhas = m_app->composicaoParaVenda(m_copao);
    QVariantList escolhas;
    for (const QVariant &v : linhas) {
        const QVariantMap l = v.toMap();
        escolhas.push_back(l.value(QStringLiteral("categoriaId")).toInt() == m_catDestilados
                               ? m_ginCaro
                               : l.value(QStringLiteral("produtoPadraoId")).toInt());
    }
    // 20 c/ml - 16 c/ml = 4 centavos por ml; x 80 ml = R$ 3,20
    QCOMPARE(m_app->precoCompostoMontado(m_copao, escolhas), qlonglong(3320));
}

// Receita antiga, sem padrão definido: não inventa ajuste nenhum.
void TstCopao::semPadraoDefinidoNaoAjustaNada()
{
    QVariantMap c = m_app->produto(m_copao);
    QVariantList comp = c.value(QStringLiteral("composicao")).toList();
    QVariantList semPadrao;
    for (const QVariant &v : comp) {
        QVariantMap l = v.toMap();
        l[QStringLiteral("produtoPadraoId")] = 0;
        semPadrao.push_back(l);
    }
    c[QStringLiteral("composicao")] = semPadrao;
    QVERIFY2(m_app->salvarProduto(c), qUtf8Printable(m_app->ultimoErro()));

    const QVariantList linhas = m_app->composicaoParaVenda(m_copao);
    QVariantList escolhas;
    for (const QVariant &v : linhas) {
        const QVariantList ps = v.toMap().value(QStringLiteral("produtos")).toList();
        escolhas.push_back(ps.isEmpty() ? 0 : ps.first().toMap().value(QStringLiteral("id")));
    }
    QCOMPARE(m_app->precoCompostoMontado(m_copao, escolhas), qlonglong(3000));
}

// O campo de preco do copao era livre para qualquer um: dava para vender o de
// R$ 30,00 por R$ 20,00 sem deixar rastro de desconto. Agora, sem permissao, a
// venda so passa pelo preco calculado.
void TstCopao::funcionarioNaoPodeMudarOPrecoDoCopao()
{
    // Restaura a composicao padrao (o teste anterior a removeu).
    QVariantMap c = m_app->produto(m_copao);
    QVariantList comp = c.value(QStringLiteral("composicao")).toList();
    QVariantList comPadrao;
    for (const QVariant &v : comp) {
        QVariantMap l = v.toMap();
        const int cat = l.value(QStringLiteral("categoriaId")).toInt();
        l[QStringLiteral("produtoPadraoId")] = cat == m_catDestilados ? m_beafeeter
                                             : cat == m_catEnergetico ? m_extraPower
                                             : cat == m_catGelo ? m_gelo : m_copo;
        comPadrao.push_back(l);
    }
    c[QStringLiteral("composicao")] = comPadrao;
    QVERIFY(m_app->salvarProduto(c));

    QVERIFY(m_app->salvarUsuario([&]{
        QVariantMap u = m_app->novoUsuario();
        u[QStringLiteral("nome")] = QStringLiteral("Balcao");
        u[QStringLiteral("login")] = QStringLiteral("balcao");
        u[QStringLiteral("perfilId")] = 2;
        return u;
    }(), QStringLiteral("balcao12345")));

    m_app->logout();
    QVERIFY(m_app->login(QStringLiteral("balcao"), QStringLiteral("balcao12345")));
    QVERIFY(m_app->abrirCaixa(QStringLiteral("50,00")));

    const QVariantList linhas = m_app->composicaoParaVenda(m_copao);
    QVariantList insumos;
    for (const QVariant &v : linhas) {
        const QVariantMap l = v.toMap();
        QVariantMap ins;
        ins[QStringLiteral("produtoId")] = l.value(QStringLiteral("produtoPadraoId"));
        ins[QStringLiteral("quantidade")] = l.value(QStringLiteral("quantidade"));
        insumos.push_back(ins);
    }

    auto vender = [&](qint64 preco) {
        QVariantMap item;
        item[QStringLiteral("produtoId")] = m_copao;
        item[QStringLiteral("embalagemId")] = 0;
        item[QStringLiteral("fator")] = 1;
        item[QStringLiteral("qtd")] = 1;
        item[QStringLiteral("precoUnit")] = static_cast<qlonglong>(preco);
        item[QStringLiteral("desconto")] = 0;
        item[QStringLiteral("insumos")] = insumos;
        QVariantMap pag;
        pag[QStringLiteral("forma")] = QStringLiteral("dinheiro");
        pag[QStringLiteral("valor")] = static_cast<qlonglong>(preco);
        QVariantMap venda;
        venda[QStringLiteral("desconto")] = 0;
        venda[QStringLiteral("clienteId")] = 0;
        venda[QStringLiteral("itens")] = QVariantList{item};
        venda[QStringLiteral("pagamentos")] = QVariantList{pag};
        return m_app->finalizarVenda(venda);
    };

    // Preco calculado: passa.
    const QVariantMap ok = vender(3000);
    QVERIFY2(ok.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(ok.value(QStringLiteral("erro")).toString()));

    // Preco mexido: recusa, e explica.
    const QVariantMap barato = vender(2000);
    QCOMPARE(barato.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(barato.value(QStringLiteral("erro")).toString().contains(QStringLiteral("preco"),
            Qt::CaseInsensitive)
            || barato.value(QStringLiteral("erro")).toString().contains(QStringLiteral("preço"),
            Qt::CaseInsensitive));

    // O dono passa.
    m_app->logout();
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));
    const QVariantMap dono = vender(2000);
    QVERIFY2(dono.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(dono.value(QStringLiteral("erro")).toString()));
}

// Venda cancelada nao pode deixar so o custo para tras. A receita ja filtrava
// por status='concluida'; o custo, nao - entao uma venda cancelada zerava a
// receita e mantinha o custo, e o lucro do dia aparecia NEGATIVO. Foi assim que
// a loja viu "-R$ 7,50" com uma unica venda, cancelada, no dia.
void TstCopao::vendaCanceladaNaoDeixaCustoNoLucro()
{
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));
    if (!m_app->caixaAberto())
        QVERIFY(m_app->abrirCaixa(QStringLiteral("100,00")));

    // Da entrada com custo, para haver custo a contabilizar.
    const auto emb = prod().obter(m_extraPower)->embalagens.first();
    QVERIFY2(m_app->registrarEntrada(m_extraPower, emb.id, 10, QStringLiteral("30,00"),
                                     QStringLiteral("carga")),
             qUtf8Printable(m_app->ultimoErro()));

    const qlonglong lucroAntes = m_app->relatorioFaturamento(0)
                                     .value(QStringLiteral("lucro")).toLongLong();

    QVariantMap item;
    item[QStringLiteral("produtoId")] = m_extraPower;
    item[QStringLiteral("embalagemId")] = emb.id;
    item[QStringLiteral("fator")] = 1;
    item[QStringLiteral("qtd")] = 1;
    item[QStringLiteral("precoUnit")] = 550;
    item[QStringLiteral("desconto")] = 0;
    QVariantMap pag;
    pag[QStringLiteral("forma")] = QStringLiteral("dinheiro");
    pag[QStringLiteral("valor")] = 550;
    QVariantMap venda;
    venda[QStringLiteral("desconto")] = 0;
    venda[QStringLiteral("clienteId")] = 0;
    venda[QStringLiteral("itens")] = QVariantList{item};
    venda[QStringLiteral("pagamentos")] = QVariantList{pag};

    const QVariantMap r = m_app->finalizarVenda(venda);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    // Cancelada: o lucro tem que VOLTAR exatamente ao que era. Antes da correcao
    // ficava 300 centavos MENOR (o custo do item vendido continuava contando,
    // enquanto a receita sumia) - e com poucas vendas no dia isso deixava o
    // numero negativo.
    const QVariantMap canc = m_app->cancelarVenda(r.value(QStringLiteral("vendaId")).toInt(),
                                                  QStringLiteral("teste"));
    QVERIFY2(canc.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(canc.value(QStringLiteral("erro")).toString()));

    const qlonglong lucroDepois = m_app->relatorioFaturamento(0)
                                      .value(QStringLiteral("lucro")).toLongLong();
    QCOMPARE(lucroDepois, lucroAntes);
}

QTEST_MAIN(TstCopao)
#include "tst_copao.moc"
