#include <QtTest>

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"
#include "services/log/LogService.h"
#include "utils/Money.h"

// Verificação geral do sistema — tudo o que foi combinado com o dono ao longo
// do projeto, conferido de ponta a ponta pelo AppBackend (o mesmo caminho das
// telas), num banco novo. Cada caso cita a regra que protege.
//
// Grupos: dinheiro · usuários e permissões · cadastro e embalagem · fator da
// embalagem · estoque e custo · custo exato em ml · aviso de custo · venda ·
// cancelamento · caixa · financeiro · clientes e fiado · relatórios · copão e
// dose · validade · backup · fotos · registro.
class TstVerificacaoGeral : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    // --- Dinheiro (Money) ---
    void dinheiro01_virgulaDecimal();
    void dinheiro02_milharECifrao();
    void dinheiro03_letraONaoViraNumero();
    void dinheiro04_tresCasasRecusado();
    void dinheiro05_formatacaoComMilhar();
    void dinheiro06_vazioInvalido();

    // --- Usuários e permissões ---
    void usuarios07_senhaErradaNaoEntra();
    void usuarios08_administradorPodeTudo();
    void usuarios09_funcionarioNaoVeFinanceiro();
    void usuarios10_funcionarioNaoAlteraProduto();
    void usuarios11_funcionarioNaoRegistraCompra();
    void usuarios12_naoFicaSemAdministrador();

    // --- Cadastro e embalagem ---
    void cadastro13_produtoNasceComUnidade();
    void cadastro14_fatorZeroRecusado();
    void cadastro15_mesmoFatorPrecoDiferenteRecusado();
    void cadastro16_mesmoFatorMesmoPrecoAceito();
    void cadastro17_precoEmCentavos();
    void cadastro18_codigoDuplicadoMensagemClara();
    void cadastro19_inativoNaoVendePeloCodigo();
    void cadastro20_compraNaoOfereceComposto();

    // --- Fator vem do cadastro (A1) ---
    void fator21_vendaUsaFatorDoCadastro();
    void fator22_compraUsaFatorDoCadastro();
    void fator23_vendaComEmbalagemDeOutroProduto();
    void fator24_entradaComEmbalagemDeOutroProduto();
    void fator25_retiradaEmCaixa();

    // --- Estoque e custo ---
    void estoque26_mediaPonderada();
    void estoque27_entradaSemCustoMantemCusto();
    void estoque28_custoIlegivelRecusado();
    void estoque29_validadeInvalidaNaoGrava();
    void estoque30_vendeSemEstoqueEFicaNegativo();
    void estoque31_compraComSaldoNegativoUsaCustoDaCompra();
    void estoque32_vendaSemEstoqueGanhaCustoDaCompra();
    void estoque33_custoZeroNaoEntraNaMedia();
    void estoque34_inventarioAjustaSaldo();
    void estoque35_retiradaAcimaDoSaldoRecusada();

    // --- Custo exato em ml ---
    void ml36_custoGuardadoExato();
    void ml37_custoExibidoArredonda();
    void ml38_relatorioDoCelularGera();

    // --- Aviso de custo fora do normal (A2) ---
    void aviso39_custoNormalNaoAvisa();
    void aviso40_custoDeLataNaCaixa();
    void aviso41_custoDeCaixaNaLata();
    void aviso42_semPrecoNaoAvisa();

    // --- Venda ---
    void venda43_trocoSoSobreDinheiro();
    void venda44_pixAMaisNaoViraTroco();
    void venda45_pagamentoInsuficiente();
    void venda46_quantidadeZeroRecusada();
    void venda47_pagamentoNegativoRecusado();
    void venda48_funcionarioSemDesconto();
    void venda49_baixaEmUnidadeBase();
    void venda50_fiadoExigeCliente();
    void venda51_fiadoAcimaDoLimite();
    void venda52_fiadoGeraContaAReceber();

    // --- Cancelamento ---
    void cancelar53_mesmoTurnoEsperadoVolta();
    void cancelar54_devolveEstoque();
    void cancelar55_cancelaFiadoAberto();
    void cancelar56_fiadoJaPagoSaiDaGaveta();
    void cancelar57_caixaFechadoComDinheiroRecusa();
    void cancelar58_exigeMotivoEPermissao();

    // --- Caixa ---
    void caixa59_aberturaIlegivelRecusada();
    void caixa60_aberturaVaziaEZero();
    void caixa61_sangriaReduzEsperado();
    void caixa62_suprimentoAumentaEsperado();
    void caixa63_recebimentoDeFiadoEmDinheiroEntraNaGaveta();
    void caixa64_fechamentoCalculaDiferenca();
    void caixa65_contagemIlegivelRecusada();

    // --- Financeiro ---
    void financeiro66_despesaInvalidaRecusada();
    void financeiro67_pagarEmDinheiroLancaSangria();
    void financeiro68_estornoDevolveAGaveta();
    void financeiro69_despesaPagaNaoSeExclui();
    void financeiro70_recebimentoParcialDeConta();
    void financeiro71_contaDeCompraNaoSeExclui();

    // --- Clientes e fiado ---
    void clientes72_historicoDoFiado();
    void clientes73_recebimentoFifo();
    void clientes74_receberMaisQueADividaLimita();
    void clientes75_resumoDoFiado();

    // --- Relatórios ---
    void relatorio76_faturamentoDoDia();
    void relatorio77_canceladaNaoConta();
    void relatorio78_diaEspecifico();
    void relatorio79_maisVendidosPorEmbalagem();
    void relatorio80_dashboardNaoContaDose();
    void relatorio81_vendaEmHoraLocal();
    void relatorio82_cadastroEmHoraLocal();

    // --- Copão e dose ---
    void composto83_doseBaixaAGarrafa();
    void composto84_doseDisponivelPelaGarrafa();
    void composto85_copaoBaixaInsumos();
    void composto86_funcionarioNaoMudaPrecoDoCopao();

    // --- Validade (lotes) ---
    void validade87_entradaComValidadeCriaLote();
    void validade88_vendaConsomeLote();
    void validade89_retiradaConsomeLote();

    // --- Backup ---
    void backup90_fazerBackupCriaArquivo();
    void backup91_recusaArquivoQueNaoEBackup();
    void backup92_listaOBackupFeito();
    void backup93_funcionarioNaoFazBackup();

    // --- Fotos ---
    void foto94_gravaReduzida();
    void foto95_heicComOrientacao();

    // --- Registro ---
    void log96_cancelamentoFicaNoRegistro();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    int m_seq = 0;

    struct Emb { const char *nome; int fator; qint64 preco; QString codigo; };
    int produto(const QString &nome, const QVector<Emb> &embs,
                const QString &unidade = QStringLiteral("unidade"));
    int emb(int produtoId, int fator);
    QString nomeUnico(const char *base) { return QStringLiteral("%1 #%2").arg(QLatin1String(base)).arg(++m_seq); }
    qint64 saldo(int produtoId);
    qint64 custoMilli(int produtoId);
    qint64 escalar(const QString &sql);
    static QVariantMap item(int pid, int embId, qint64 qtd, qint64 preco, int fatorTela = 1);
    static QVariantMap pag(const char *forma, qint64 valor);
    QVariantMap vender(const QVariantList &itens, const QVariantList &pags,
                       int clienteId = 0, qint64 desconto = 0);
    int venderSimples(int pid, qint64 qtd, qint64 preco, const char *forma = "dinheiro");
    bool entrar(int pid, int embId, int qtd, const QString &custo);
    int cliente(const QString &nome, qint64 limite);
    void garantirCaixa();
    void comoDono();
    void comoFuncionario();
};

// ============================================================ infraestrutura

void TstVerificacaoGeral::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);   // backup/log/relatório longe dos dados reais
    QCoreApplication::setOrganizationName(QStringLiteral("DistribuidoraVerificacao"));

    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->precisaCriarAdmin());
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"), QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    QVariantMap f = m_app->novoUsuario();
    f[QStringLiteral("nome")] = QStringLiteral("Balcão");
    f[QStringLiteral("login")] = QStringLiteral("balcao");
    f[QStringLiteral("perfilId")] = 2;
    QVERIFY2(m_app->salvarUsuario(f, QStringLiteral("balcao12345")), qUtf8Printable(m_app->ultimoErro()));

    QVERIFY(m_app->abrirCaixa(QStringLiteral("100,00")));
}

void TstVerificacaoGeral::cleanup()
{
    comoDono();
    garantirCaixa();
}

void TstVerificacaoGeral::comoDono()
{
    if (m_app->usuarioAtual().value(QStringLiteral("login")).toString() != QStringLiteral("dono"))
        QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));
}

void TstVerificacaoGeral::comoFuncionario()
{
    m_app->logout();
    QVERIFY(m_app->login(QStringLiteral("balcao"), QStringLiteral("balcao12345")));
}

void TstVerificacaoGeral::garantirCaixa()
{
    if (!m_app->caixaAberto())
        QVERIFY(m_app->abrirCaixa(QStringLiteral("0,00")));
}

int TstVerificacaoGeral::produto(const QString &nome, const QVector<Emb> &embs, const QString &unidade)
{
    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = nome;
    p[QStringLiteral("categoriaId")] = m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();
    p[QStringLiteral("unidadeBase")] = unidade;
    QVariantList lista;
    for (const Emb &e : embs) {
        lista.push_back(QVariantMap{{QStringLiteral("id"), 0},
                                    {QStringLiteral("nome"), QString::fromUtf8(e.nome)},
                                    {QStringLiteral("fator"), e.fator},
                                    {QStringLiteral("codigoBarras"), e.codigo},
                                    {QStringLiteral("preco"), e.preco},
                                    {QStringLiteral("custo"), -1}});
    }
    p[QStringLiteral("embalagens")] = lista;
    if (!m_app->salvarProduto(p))
        return 0;
    for (const QVariant &v : m_app->buscarProdutosPorNome(nome))
        if (v.toMap().value(QStringLiteral("nome")).toString() == nome)
            return v.toMap().value(QStringLiteral("produtoId")).toInt();
    return 0;
}

int TstVerificacaoGeral::emb(int produtoId, int fator)
{
    for (const QVariant &v : m_app->embalagensDe(produtoId))
        if (v.toMap().value(QStringLiteral("fator")).toInt() == fator)
            return v.toMap().value(QStringLiteral("id")).toInt();
    return 0;
}

qint64 TstVerificacaoGeral::escalar(const QString &sql)
{
    QSqlQuery q(m_db.connection());
    return (q.exec(sql) && q.next()) ? q.value(0).toLongLong() : -999999;
}

qint64 TstVerificacaoGeral::saldo(int produtoId)
{
    return escalar(QStringLiteral("SELECT COALESCE((SELECT quantidade_atual FROM estoque WHERE produto_id = %1), 0)")
                       .arg(produtoId));
}

qint64 TstVerificacaoGeral::custoMilli(int produtoId)
{
    return escalar(QStringLiteral("SELECT custo_medio_unitario FROM estoque WHERE produto_id = %1").arg(produtoId));
}

QVariantMap TstVerificacaoGeral::item(int pid, int embId, qint64 qtd, qint64 preco, int fatorTela)
{
    return {{QStringLiteral("produtoId"), pid}, {QStringLiteral("embalagemId"), embId},
            {QStringLiteral("fator"), fatorTela}, {QStringLiteral("qtd"), qtd},
            {QStringLiteral("precoUnit"), preco}, {QStringLiteral("desconto"), 0}};
}

QVariantMap TstVerificacaoGeral::pag(const char *forma, qint64 valor)
{
    return {{QStringLiteral("forma"), QString::fromLatin1(forma)}, {QStringLiteral("valor"), valor}};
}

QVariantMap TstVerificacaoGeral::vender(const QVariantList &itens, const QVariantList &pags,
                                        int clienteId, qint64 desconto)
{
    return m_app->finalizarVenda({{QStringLiteral("desconto"), desconto},
                                  {QStringLiteral("clienteId"), clienteId},
                                  {QStringLiteral("itens"), itens},
                                  {QStringLiteral("pagamentos"), pags}});
}

int TstVerificacaoGeral::venderSimples(int pid, qint64 qtd, qint64 preco, const char *forma)
{
    const QVariantMap r = vender({item(pid, emb(pid, 1), qtd, preco)}, {pag(forma, qtd * preco)});
    return r.value(QStringLiteral("ok")).toBool() ? r.value(QStringLiteral("vendaId")).toInt() : 0;
}

bool TstVerificacaoGeral::entrar(int pid, int embId, int qtd, const QString &custo)
{
    return m_app->registrarEntrada(pid, embId, qtd, custo, QString());
}

int TstVerificacaoGeral::cliente(const QString &nome, qint64 limite)
{
    QVariantMap c = m_app->novoCliente();
    c[QStringLiteral("nome")] = nome;
    c[QStringLiteral("limite")] = limite;
    if (!m_app->salvarCliente(c))
        return 0;
    for (const QVariant &v : m_app->clientesLista())
        if (v.toMap().value(QStringLiteral("nome")).toString() == nome)
            return v.toMap().value(QStringLiteral("id")).toInt();
    return 0;
}

// ============================================================ dinheiro

void TstVerificacaoGeral::dinheiro01_virgulaDecimal()
{
    QCOMPARE(Money::parse(QStringLiteral("12,50")).value_or(-1), Q_INT64_C(1250));
    QCOMPARE(Money::parse(QStringLiteral("12.5")).value_or(-1), Q_INT64_C(1250));
}

void TstVerificacaoGeral::dinheiro02_milharECifrao()
{
    QCOMPARE(Money::parse(QStringLiteral("R$ 1.234,56")).value_or(-1), Q_INT64_C(123456));
}

// "1OO" com a letra O virava R$ 1,00 — diferença de caixa que ninguém explicava.
void TstVerificacaoGeral::dinheiro03_letraONaoViraNumero()
{
    QVERIFY(!Money::parse(QStringLiteral("1OO")).has_value());
    QCOMPARE(m_app->parseDinheiro(QStringLiteral("4,5O")), Q_INT64_C(-1));
}

void TstVerificacaoGeral::dinheiro04_tresCasasRecusado()
{
    QVERIFY(!Money::parse(QStringLiteral("12,345")).has_value());
}

void TstVerificacaoGeral::dinheiro05_formatacaoComMilhar()
{
    QCOMPARE(Money::format(123456789), QStringLiteral("R$ 1.234.567,89"));
    QCOMPARE(Money::formatPlain(5), QStringLiteral("0,05"));
}

void TstVerificacaoGeral::dinheiro06_vazioInvalido()
{
    QVERIFY(!Money::parse(QStringLiteral("   ")).has_value());
    QVERIFY(!Money::parse(QStringLiteral("R$")).has_value());
}

// ============================================================ usuários

void TstVerificacaoGeral::usuarios07_senhaErradaNaoEntra()
{
    QVERIFY(!m_app->login(QStringLiteral("dono"), QStringLiteral("errada")));
    QVERIFY(!m_app->login(QStringLiteral("ninguem"), QStringLiteral("dono12345")));
}

void TstVerificacaoGeral::usuarios08_administradorPodeTudo()
{
    for (const char *k : {"vende", "edita_produto", "ve_financeiro", "gerencia_usuarios",
                          "pode_cancelar_venda", "pode_dar_desconto", "ajusta_estoque"})
        QVERIFY2(m_app->temPermissao(QString::fromLatin1(k)), k);
}

void TstVerificacaoGeral::usuarios09_funcionarioNaoVeFinanceiro()
{
    comoFuncionario();
    QVERIFY(m_app->temPermissao(QStringLiteral("vende")));
    QVERIFY(m_app->temPermissao(QStringLiteral("recebe_mercadoria")));
    for (const char *k : {"ve_financeiro", "gerencia_usuarios", "edita_produto",
                          "pode_dar_desconto", "ajusta_estoque", "pode_cancelar_venda"})
        QVERIFY2(!m_app->temPermissao(QString::fromLatin1(k)), k);
}

void TstVerificacaoGeral::usuarios10_funcionarioNaoAlteraProduto()
{
    comoFuncionario();
    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = QStringLiteral("Proibido");
    QVERIFY(!m_app->salvarProduto(p));
    QVERIFY(!m_app->registrarInventario(1, 10, QStringLiteral("x")));
}

void TstVerificacaoGeral::usuarios11_funcionarioNaoRegistraCompra()
{
    const int pid = produto(nomeUnico("Compra proibida"), {{"Unidade", 1, 500, {}}});
    comoFuncionario();
    const QVariantMap r = m_app->registrarCompra(
        {{QStringLiteral("itens"), QVariantList{QVariantMap{{QStringLiteral("produtoId"), pid},
                                                            {QStringLiteral("embalagemId"), emb(pid, 1)},
                                                            {QStringLiteral("qtd"), 5},
                                                            {QStringLiteral("custo"), 300}}}}});
    QVERIFY(!r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(pid), Q_INT64_C(0));
    QVERIFY(!m_app->pagarConta(1));
    QVERIFY(!m_app->criarDespesa(QStringLiteral("x"), QStringLiteral("10,00"), QString()));
}

void TstVerificacaoGeral::usuarios12_naoFicaSemAdministrador()
{
    const int dono = m_app->usuarioAtual().value(QStringLiteral("id")).toInt();
    QVariantMap u = m_app->usuario(dono);
    u[QStringLiteral("perfilId")] = 2;
    QVERIFY(!m_app->salvarUsuario(u, QString()));
    QVERIFY(m_app->ultimoErro().contains(QStringLiteral("único administrador")));
    QVERIFY(!m_app->inativarUsuario(dono));   // e também não desativa a si mesmo
}

// ============================================================ cadastro

void TstVerificacaoGeral::cadastro13_produtoNasceComUnidade()
{
    const QVariantList embs = m_app->novoProduto().value(QStringLiteral("embalagens")).toList();
    QCOMPARE(embs.size(), 1);
    QCOMPARE(embs.first().toMap().value(QStringLiteral("fator")).toInt(), 1);
}

void TstVerificacaoGeral::cadastro14_fatorZeroRecusado()
{
    QCOMPARE(produto(nomeUnico("Fator zero"), {{"Unidade", 1, 450, {}}, {"Caixinha", 0, 4800, {}}}), 0);
    QVERIFY(m_app->ultimoErro().contains(QStringLiteral("Informe o fator")));
}

// O caso da ORIGINAL 350 ML na loja: caixinha com fator 1 e preço de caixa.
void TstVerificacaoGeral::cadastro15_mesmoFatorPrecoDiferenteRecusado()
{
    QCOMPARE(produto(nomeUnico("Original"), {{"Unidade", 1, 550, {}}, {"CAIXINHA", 1, 6400, {}}}), 0);
    QVERIFY(m_app->ultimoErro().contains(QStringLiteral("mesmo fator")));
}

void TstVerificacaoGeral::cadastro16_mesmoFatorMesmoPrecoAceito()
{
    QVERIFY(produto(nomeUnico("Dois codigos"), {{"Lata", 1, 450, {}}, {"Lata B", 1, 450, {}}}) > 0);
}

void TstVerificacaoGeral::cadastro17_precoEmCentavos()
{
    const int pid = produto(nomeUnico("Preco"), {{"Unidade", 1, 1234, {}}});
    QCOMPARE(m_app->embalagensDe(pid).first().toMap().value(QStringLiteral("preco")).toLongLong(),
             Q_INT64_C(1234));
}

// O erro cru do SQLite chegava na tela do balcão.
void TstVerificacaoGeral::cadastro18_codigoDuplicadoMensagemClara()
{
    QVERIFY(produto(nomeUnico("Cod A"), {{"Unidade", 1, 100, QStringLiteral("7891000")}}) > 0);
    QCOMPARE(produto(nomeUnico("Cod B"), {{"Unidade", 1, 100, QStringLiteral("7891000")}}), 0);
    QVERIFY2(m_app->ultimoErro().contains(QStringLiteral("código de barras")),
             qUtf8Printable(m_app->ultimoErro()));
}

void TstVerificacaoGeral::cadastro19_inativoNaoVendePeloCodigo()
{
    const int pid = produto(nomeUnico("Velho"), {{"Unidade", 1, 100, QStringLiteral("7892000")}});
    QVERIFY(m_app->buscarProdutoPorCodigo(QStringLiteral("7892000")).value(QStringLiteral("encontrado")).toBool());
    QVERIFY(m_app->inativarProduto(pid));
    QVERIFY(!m_app->buscarProdutoPorCodigo(QStringLiteral("7892000")).value(QStringLiteral("encontrado")).toBool());
    QVERIFY(produto(nomeUnico("Substituto"), {{"Unidade", 1, 100, QStringLiteral("7892000")}}) > 0);
}

void TstVerificacaoGeral::cadastro20_compraNaoOfereceComposto()
{
    QVariantMap c = m_app->novoProduto();
    c[QStringLiteral("nome")] = QStringLiteral("Zcomposto busca");
    c[QStringLiteral("composto")] = true;
    QVERIFY(m_app->salvarProduto(c));
    QVERIFY(!m_app->buscarProdutosPorNome(QStringLiteral("Zcomposto busca"), true).isEmpty());
    QVERIFY(m_app->buscarProdutosPorNome(QStringLiteral("Zcomposto busca"), false).isEmpty());
}

// ============================================================ fator (A1)

void TstVerificacaoGeral::fator21_vendaUsaFatorDoCadastro()
{
    const int pid = produto(nomeUnico("Imperio"), {{"Unidade", 1, 450, {}}, {"Caixinha", 12, 4800, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 30, QStringLiteral("3,00")));
    const QVariantMap r = vender({item(pid, emb(pid, 12), 1, 4800, /*tela mente=*/1)}, {pag("pix", 4800)});
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(), qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    QCOMPARE(saldo(pid), Q_INT64_C(18));
}

void TstVerificacaoGeral::fator22_compraUsaFatorDoCadastro()
{
    const int pid = produto(nomeUnico("Palheiro"), {{"Unidade", 1, 200, {}}, {"BOX", 20, 2700, {}}});
    const QVariantMap r = m_app->registrarCompra(
        {{QStringLiteral("itens"), QVariantList{QVariantMap{{QStringLiteral("produtoId"), pid},
                                                            {QStringLiteral("embalagemId"), emb(pid, 20)},
                                                            {QStringLiteral("fator"), 1},
                                                            {QStringLiteral("qtd"), 2},
                                                            {QStringLiteral("custo"), 2000}}}}});
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(), qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    QCOMPARE(saldo(pid), Q_INT64_C(40));
    QCOMPARE(custoMilli(pid), Q_INT64_C(100000));   // R$ 1,00 por unidade
}

void TstVerificacaoGeral::fator23_vendaComEmbalagemDeOutroProduto()
{
    const int a = produto(nomeUnico("Prod A"), {{"Unidade", 1, 100, {}}, {"Cx", 6, 500, {}}});
    const int b = produto(nomeUnico("Prod B"), {{"Unidade", 1, 100, {}}});
    const QVariantMap r = vender({item(b, emb(a, 6), 1, 500)}, {pag("pix", 500)});
    QVERIFY(!r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(b), Q_INT64_C(0));
}

void TstVerificacaoGeral::fator24_entradaComEmbalagemDeOutroProduto()
{
    const int a = produto(nomeUnico("Ent A"), {{"Unidade", 1, 100, {}}, {"Cx", 6, 500, {}}});
    const int b = produto(nomeUnico("Ent B"), {{"Unidade", 1, 100, {}}});
    QVERIFY(!entrar(b, emb(a, 6), 1, QStringLiteral("3,00")));
    QCOMPARE(saldo(b), Q_INT64_C(0));
}

void TstVerificacaoGeral::fator25_retiradaEmCaixa()
{
    const int pid = produto(nomeUnico("Retira"), {{"Unidade", 1, 100, {}}, {"Cx", 12, 1000, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 30, QStringLiteral("1,00")));
    QVERIFY(m_app->registrarRetirada(pid, emb(pid, 12), 1, QStringLiteral("quebra")));
    QCOMPARE(saldo(pid), Q_INT64_C(18));
}

// ============================================================ estoque e custo

void TstVerificacaoGeral::estoque26_mediaPonderada()
{
    const int pid = produto(nomeUnico("Media"), {{"Unidade", 1, 900, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("3,00")));
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("5,00")));
    QCOMPARE(custoMilli(pid), Q_INT64_C(400000));
}

void TstVerificacaoGeral::estoque27_entradaSemCustoMantemCusto()
{
    const int pid = produto(nomeUnico("Mantem"), {{"Unidade", 1, 900, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("3,00")));
    QVERIFY(entrar(pid, emb(pid, 1), 10, QString()));
    QCOMPARE(custoMilli(pid), Q_INT64_C(300000));
    QCOMPARE(saldo(pid), Q_INT64_C(20));
}

void TstVerificacaoGeral::estoque28_custoIlegivelRecusado()
{
    const int pid = produto(nomeUnico("Ilegivel"), {{"Unidade", 1, 900, {}}});
    QVERIFY(!entrar(pid, emb(pid, 1), 10, QStringLiteral("3,OO")));
    QCOMPARE(saldo(pid), Q_INT64_C(0));
}

// Validade conferida DEPOIS de gravar: quem tentava de novo dava entrada 2×.
void TstVerificacaoGeral::estoque29_validadeInvalidaNaoGrava()
{
    const int pid = produto(nomeUnico("Validade ruim"), {{"Unidade", 1, 900, {}}});
    QVERIFY(!m_app->registrarEntrada(pid, emb(pid, 1), 5, QStringLiteral("1,00"), QString(),
                                     QStringLiteral("31/02/2026")));
    QCOMPARE(saldo(pid), Q_INT64_C(0));
}

void TstVerificacaoGeral::estoque30_vendeSemEstoqueEFicaNegativo()
{
    const int pid = produto(nomeUnico("Sem estoque"), {{"Unidade", 1, 500, {}}});
    QVERIFY(venderSimples(pid, 3, 500) > 0);   // o PDV não trava (decisão do dono)
    QCOMPARE(saldo(pid), Q_INT64_C(-3));
}

// O "lucro que despencava e voltava": média com saldo negativo dava R$ 18,00.
void TstVerificacaoGeral::estoque31_compraComSaldoNegativoUsaCustoDaCompra()
{
    const int pid = produto(nomeUnico("Negativo"), {{"Unidade", 1, 500, {}}});
    QVERIFY(venderSimples(pid, 20, 500) > 0);
    QVERIFY(entrar(pid, emb(pid, 1), 24, QStringLiteral("3,00")));
    QCOMPARE(custoMilli(pid), Q_INT64_C(300000));
    QCOMPARE(saldo(pid), Q_INT64_C(4));
}

void TstVerificacaoGeral::estoque32_vendaSemEstoqueGanhaCustoDaCompra()
{
    const int pid = produto(nomeUnico("Pendente"), {{"Unidade", 1, 1000, {}}});
    const int venda = venderSimples(pid, 5, 1000);
    QVERIFY(venda > 0);
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("4,00")));
    QCOMPARE(escalar(QStringLiteral("SELECT SUM(-quantidade * custo_unit) FROM movimentacoes_estoque "
                                    "WHERE origem = 'venda:%1'").arg(venda)),
             Q_INT64_C(5 * 400000));
}

void TstVerificacaoGeral::estoque33_custoZeroNaoEntraNaMedia()
{
    const int pid = produto(nomeUnico("Sem custo"), {{"Unidade", 1, 1000, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 10, QString()));
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("3,00")));
    QCOMPARE(custoMilli(pid), Q_INT64_C(300000));
}

void TstVerificacaoGeral::estoque34_inventarioAjustaSaldo()
{
    const int pid = produto(nomeUnico("Inventario"), {{"Unidade", 1, 1000, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("3,00")));
    QVERIFY(m_app->registrarInventario(pid, 7, QStringLiteral("contagem")));
    QCOMPARE(saldo(pid), Q_INT64_C(7));
    QCOMPARE(escalar(QStringLiteral("SELECT SUM(quantidade) FROM movimentacoes_estoque WHERE produto_id = %1").arg(pid)),
             Q_INT64_C(7));
}

void TstVerificacaoGeral::estoque35_retiradaAcimaDoSaldoRecusada()
{
    const int pid = produto(nomeUnico("Retira demais"), {{"Unidade", 1, 1000, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 2, QStringLiteral("3,00")));
    QVERIFY(!m_app->registrarRetirada(pid, emb(pid, 1), 5, QStringLiteral("x")));
    QCOMPARE(saldo(pid), Q_INT64_C(2));
}

// ============================================================ custo em ml

void TstVerificacaoGeral::ml36_custoGuardadoExato()
{
    const int pid = produto(nomeUnico("Black Stone"), {{"Garrafa", 1000, 4500, {}}}, QStringLiteral("ml"));
    QVERIFY(entrar(pid, emb(pid, 1000), 1, QStringLiteral("18,99")));
    const QVariantMap it = m_app->itemEstoque(pid);
    // R$ 18,99 por garrafa de 1000 ml = 1,899 centavos por ml = 1899 milésimos.
    QCOMPARE(it.value(QStringLiteral("custoMedioMilli")).toLongLong(), Q_INT64_C(1899));
    // Custo por ml truncado em centavos (1) dava R$ 10,00 na garrafa; o que a
    // Compra sugere agora sai do valor exato: R$ 18,99.
    QCOMPARE(qRound64(it.value(QStringLiteral("custoMedioMilli")).toLongLong() * 1000 / 1000.0), Q_INT64_C(1899));
    QCOMPARE(it.value(QStringLiteral("custoMedio")).toLongLong(), Q_INT64_C(2));   // exibição por ml
}

void TstVerificacaoGeral::ml37_custoExibidoArredonda()
{
    const int pid = produto(nomeUnico("Arredonda"), {{"Unidade", 1, 900, {}}, {"Cx", 3, 2500, {}}});
    QVERIFY(entrar(pid, emb(pid, 3), 1, QStringLiteral("10,00")));   // 3,333... por unidade
    QCOMPARE(m_app->itemEstoque(pid).value(QStringLiteral("custoMedio")).toLongLong(), Q_INT64_C(333));
    // Média de 3 un. a 3,3333 com 1 un. a 3,35 = 3,3375: arredonda para 3,34
    // (truncando daria 3,33).
    QVERIFY(entrar(pid, emb(pid, 1), 1, QStringLiteral("3,35")));
    QCOMPARE(m_app->itemEstoque(pid).value(QStringLiteral("custoMedio")).toLongLong(), Q_INT64_C(334));
}

void TstVerificacaoGeral::ml38_relatorioDoCelularGera()
{
    const QVariantMap r = m_app->gerarRelatorioCelular();
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(), qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    QFile f(r.value(QStringLiteral("caminho")).toString());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray html = f.readAll();
    QVERIFY(html.contains("estoqueValor"));
    QVERIFY(!html.contains("</script><"));   // nome de produto não quebra o HTML
}

// ============================================================ aviso de custo

void TstVerificacaoGeral::aviso39_custoNormalNaoAvisa()
{
    const int pid = produto(nomeUnico("Aviso"), {{"Unidade", 1, 450, {}}, {"Cx", 12, 4800, {}}});
    QCOMPARE(m_app->avaliarCusto(pid, emb(pid, 12), QStringLiteral("36,00")).value(QStringLiteral("nivel")).toString(),
             QString());
}

void TstVerificacaoGeral::aviso40_custoDeLataNaCaixa()
{
    const int pid = produto(nomeUnico("Aviso baixo"), {{"Unidade", 1, 450, {}}, {"Cx", 12, 4800, {}}});
    QCOMPARE(m_app->avaliarCusto(pid, emb(pid, 12), QStringLiteral("3,00")).value(QStringLiteral("nivel")).toString(),
             QStringLiteral("baixo"));
}

void TstVerificacaoGeral::aviso41_custoDeCaixaNaLata()
{
    const int pid = produto(nomeUnico("Aviso alto"), {{"Unidade", 1, 450, {}}, {"Cx", 12, 4800, {}}});
    QCOMPARE(m_app->avaliarCusto(pid, emb(pid, 1), QStringLiteral("36,00")).value(QStringLiteral("nivel")).toString(),
             QStringLiteral("alto"));
}

void TstVerificacaoGeral::aviso42_semPrecoNaoAvisa()
{
    const int pid = produto(nomeUnico("Sem preco"), {{"Unidade", 1, 0, {}}});
    QCOMPARE(m_app->avaliarCusto(pid, emb(pid, 1), QStringLiteral("99,00")).value(QStringLiteral("nivel")).toString(),
             QString());
}

// ============================================================ venda

void TstVerificacaoGeral::venda43_trocoSoSobreDinheiro()
{
    const int pid = produto(nomeUnico("Troco"), {{"Unidade", 1, 750, {}}});
    const QVariantMap r = vender({item(pid, emb(pid, 1), 1, 750)}, {pag("dinheiro", 1000)});
    QVERIFY(r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(r.value(QStringLiteral("troco")).toLongLong(), Q_INT64_C(250));
}

// Pix a mais virava "troco" e o fechamento tirava dinheiro que nunca entrou.
void TstVerificacaoGeral::venda44_pixAMaisNaoViraTroco()
{
    const int pid = produto(nomeUnico("Pix a mais"), {{"Unidade", 1, 750, {}}});
    const QVariantMap r = vender({item(pid, emb(pid, 1), 1, 750)}, {pag("pix", 1000)});
    QVERIFY(r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(r.value(QStringLiteral("troco")).toLongLong(), Q_INT64_C(0));
}

void TstVerificacaoGeral::venda45_pagamentoInsuficiente()
{
    const int pid = produto(nomeUnico("Falta"), {{"Unidade", 1, 750, {}}});
    QVERIFY(!vender({item(pid, emb(pid, 1), 1, 750)}, {pag("dinheiro", 500)}).value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(pid), Q_INT64_C(0));
}

void TstVerificacaoGeral::venda46_quantidadeZeroRecusada()
{
    const int pid = produto(nomeUnico("Qtd zero"), {{"Unidade", 1, 750, {}}});
    QVERIFY(!vender({item(pid, emb(pid, 1), 0, 750)}, {pag("dinheiro", 750)}).value(QStringLiteral("ok")).toBool());
    QVERIFY(!vender({item(pid, emb(pid, 1), -2, 750)}, {pag("dinheiro", 750)}).value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(pid), Q_INT64_C(0));
}

void TstVerificacaoGeral::venda47_pagamentoNegativoRecusado()
{
    const int pid = produto(nomeUnico("Pag neg"), {{"Unidade", 1, 750, {}}});
    QVERIFY(!vender({item(pid, emb(pid, 1), 1, 750)}, {pag("dinheiro", 1500), pag("pix", -750)})
                 .value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::venda48_funcionarioSemDesconto()
{
    const int pid = produto(nomeUnico("Desconto"), {{"Unidade", 1, 1000, {}}});
    comoFuncionario();
    const QVariantMap r = vender({item(pid, emb(pid, 1), 1, 1000)}, {pag("dinheiro", 900)}, 0, 100);
    QVERIFY(!r.value(QStringLiteral("ok")).toBool());
    QVERIFY(r.value(QStringLiteral("erro")).toString().contains(QStringLiteral("desconto")));
}

void TstVerificacaoGeral::venda49_baixaEmUnidadeBase()
{
    const int pid = produto(nomeUnico("Base"), {{"Unidade", 1, 500, {}}, {"Fardo", 6, 2800, {}}});
    const QVariantMap r = vender({item(pid, emb(pid, 6), 2, 2800, 6)}, {pag("pix", 5600)});
    QVERIFY(r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(escalar(QStringLiteral("SELECT qtd_unidade_base FROM venda_itens WHERE venda_id = %1")
                         .arg(r.value(QStringLiteral("vendaId")).toInt())),
             Q_INT64_C(12));
}

void TstVerificacaoGeral::venda50_fiadoExigeCliente()
{
    const int pid = produto(nomeUnico("Fiado sem cli"), {{"Unidade", 1, 500, {}}});
    QVERIFY(!vender({item(pid, emb(pid, 1), 1, 500)}, {pag("fiado", 500)}).value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::venda51_fiadoAcimaDoLimite()
{
    const int pid = produto(nomeUnico("Fiado limite"), {{"Unidade", 1, 500, {}}});
    const int cli = cliente(nomeUnico("Cli limite"), 1000);
    QVERIFY(vender({item(pid, emb(pid, 1), 1, 500)}, {pag("fiado", 500)}, cli).value(QStringLiteral("ok")).toBool());
    QVERIFY(!vender({item(pid, emb(pid, 1), 2, 500)}, {pag("fiado", 1000)}, cli).value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::venda52_fiadoGeraContaAReceber()
{
    const int pid = produto(nomeUnico("Fiado conta"), {{"Unidade", 1, 500, {}}});
    const int cli = cliente(nomeUnico("Cli conta"), 5000);
    QVERIFY(vender({item(pid, emb(pid, 1), 3, 500)}, {pag("fiado", 1500)}, cli).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->cliente(cli).value(QStringLiteral("saldo")).toLongLong(), Q_INT64_C(1500));
}

// ============================================================ cancelamento

void TstVerificacaoGeral::cancelar53_mesmoTurnoEsperadoVolta()
{
    const int pid = produto(nomeUnico("Canc 1"), {{"Unidade", 1, 1000, {}}});
    const qint64 antes = m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    const int v = venderSimples(pid, 2, 1000);
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(), antes + 2000);
    QVERIFY(m_app->cancelarVenda(v, QStringLiteral("erro")).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(), antes);
}

void TstVerificacaoGeral::cancelar54_devolveEstoque()
{
    const int pid = produto(nomeUnico("Canc 2"), {{"Unidade", 1, 1000, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 10, QStringLiteral("5,00")));
    const int v = venderSimples(pid, 4, 1000, "pix");
    QCOMPARE(saldo(pid), Q_INT64_C(6));
    QVERIFY(m_app->cancelarVenda(v, QStringLiteral("desistiu")).value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(pid), Q_INT64_C(10));
    QCOMPARE(m_app->relatorioFaturamento(0).value(QStringLiteral("total")).toLongLong() >= 0, true);
}

void TstVerificacaoGeral::cancelar55_cancelaFiadoAberto()
{
    const int pid = produto(nomeUnico("Canc 3"), {{"Unidade", 1, 1000, {}}});
    const int cli = cliente(nomeUnico("Cli canc"), 5000);
    const QVariantMap r = vender({item(pid, emb(pid, 1), 1, 1000)}, {pag("fiado", 1000)}, cli);
    QVERIFY(m_app->cancelarVenda(r.value(QStringLiteral("vendaId")).toInt(), QStringLiteral("x"))
                .value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->cliente(cli).value(QStringLiteral("saldo")).toLongLong(), Q_INT64_C(0));
}

// Venda nº 3 da loja: fiado recebido e depois cancelado, sem devolução.
void TstVerificacaoGeral::cancelar56_fiadoJaPagoSaiDaGaveta()
{
    const int pid = produto(nomeUnico("Canc 4"), {{"Unidade", 1, 821, {}}});
    const int cli = cliente(nomeUnico("Cli pagou"), 5000);
    const QVariantMap r = vender({item(pid, emb(pid, 1), 1, 821)}, {pag("fiado", 821)}, cli);
    QVERIFY(m_app->receberDeCliente(cli, QString(), QStringLiteral("dinheiro")).value(QStringLiteral("ok")).toBool());
    const qint64 sangrias = m_app->caixaResumo().value(QStringLiteral("sangrias")).toLongLong();
    const QVariantMap c = m_app->cancelarVenda(r.value(QStringLiteral("vendaId")).toInt(), QStringLiteral("teste"));
    QVERIFY(c.value(QStringLiteral("ok")).toBool());
    QVERIFY(c.value(QStringLiteral("aviso")).toString().contains(QStringLiteral("8,21")));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("sangrias")).toLongLong(), sangrias + 821);
}

void TstVerificacaoGeral::cancelar57_caixaFechadoComDinheiroRecusa()
{
    const int pid = produto(nomeUnico("Canc 5"), {{"Unidade", 1, 1000, {}}});
    const int v = venderSimples(pid, 1, 1000);
    QVERIFY(m_app->fecharCaixa(QStringLiteral("0,00")).value(QStringLiteral("ok")).toBool());
    const QVariantMap c = m_app->cancelarVenda(v, QStringLiteral("x"));
    QVERIFY(!c.value(QStringLiteral("ok")).toBool());
    QVERIFY(c.value(QStringLiteral("erro")).toString().contains(QStringLiteral("Abra o caixa")));
    garantirCaixa();
    QVERIFY(m_app->cancelarVenda(v, QStringLiteral("x")).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("sangrias")).toLongLong(), Q_INT64_C(1000));
}

void TstVerificacaoGeral::cancelar58_exigeMotivoEPermissao()
{
    const int pid = produto(nomeUnico("Canc 6"), {{"Unidade", 1, 1000, {}}});
    const int v = venderSimples(pid, 1, 1000, "pix");
    QVERIFY(!m_app->cancelarVenda(v, QStringLiteral("   ")).value(QStringLiteral("ok")).toBool());
    comoFuncionario();
    QVERIFY(!m_app->cancelarVenda(v, QStringLiteral("x")).value(QStringLiteral("ok")).toBool());
    comoDono();
    QVERIFY(m_app->cancelarVenda(v, QStringLiteral("x")).value(QStringLiteral("ok")).toBool());
    QVERIFY(!m_app->cancelarVenda(v, QStringLiteral("de novo")).value(QStringLiteral("ok")).toBool());
}

// ============================================================ caixa

void TstVerificacaoGeral::caixa59_aberturaIlegivelRecusada()
{
    QVERIFY(m_app->fecharCaixa(QStringLiteral("0,00")).value(QStringLiteral("ok")).toBool());
    QVERIFY(!m_app->abrirCaixa(QStringLiteral("1OO")));
    QVERIFY(!m_app->caixaAberto());
}

void TstVerificacaoGeral::caixa60_aberturaVaziaEZero()
{
    if (m_app->caixaAberto())
        QVERIFY(m_app->fecharCaixa(QStringLiteral("0,00")).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_app->abrirCaixa(QString()));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("abertura")).toLongLong(), Q_INT64_C(0));
}

void TstVerificacaoGeral::caixa61_sangriaReduzEsperado()
{
    const qint64 a = m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    QVERIFY(m_app->registrarSangria(QStringLiteral("15,00"), QStringLiteral("banco")));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(), a - 1500);
    QVERIFY(!m_app->registrarSangria(QStringLiteral("0"), QStringLiteral("x")));
}

void TstVerificacaoGeral::caixa62_suprimentoAumentaEsperado()
{
    const qint64 a = m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    QVERIFY(m_app->registrarSuprimento(QStringLiteral("20,00"), QStringLiteral("troco")));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(), a + 2000);
}

void TstVerificacaoGeral::caixa63_recebimentoDeFiadoEmDinheiroEntraNaGaveta()
{
    const int pid = produto(nomeUnico("Receb"), {{"Unidade", 1, 1000, {}}});
    const int cli = cliente(nomeUnico("Cli receb"), 5000);
    QVERIFY(vender({item(pid, emb(pid, 1), 1, 1000)}, {pag("fiado", 1000)}, cli).value(QStringLiteral("ok")).toBool());
    const qint64 a = m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    QVERIFY(m_app->receberDeCliente(cli, QStringLiteral("4,00"), QStringLiteral("dinheiro")).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_app->receberDeCliente(cli, QStringLiteral("3,00"), QStringLiteral("pix")).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(), a + 400);
}

void TstVerificacaoGeral::caixa64_fechamentoCalculaDiferenca()
{
    const qint64 esperado = m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    const QVariantMap r = m_app->fecharCaixa(Money::formatPlain(esperado - 250));
    QVERIFY(r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(r.value(QStringLiteral("esperado")).toLongLong(), esperado);
    QCOMPARE(r.value(QStringLiteral("diferenca")).toLongLong(), Q_INT64_C(-250));
    QVERIFY(!m_app->caixaAberto());
}

void TstVerificacaoGeral::caixa65_contagemIlegivelRecusada()
{
    QVERIFY(!m_app->fecharCaixa(QStringLiteral("25O,00")).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_app->caixaAberto());
}

// ============================================================ financeiro

void TstVerificacaoGeral::financeiro66_despesaInvalidaRecusada()
{
    QVERIFY(!m_app->criarDespesa(QStringLiteral("Luz"), QStringLiteral("0"), QString()));
    QVERIFY(!m_app->criarDespesa(QString(), QStringLiteral("10,00"), QString()));
    QVERIFY(!m_app->criarDespesa(QStringLiteral("Luz"), QStringLiteral("10,00"), QStringLiteral("20260829")));
}

static int ultimaContaPagar(const QSqlDatabase &db)
{
    QSqlQuery q(db);
    return (q.exec(QStringLiteral("SELECT MAX(id) FROM contas_pagar")) && q.next()) ? q.value(0).toInt() : 0;
}

void TstVerificacaoGeral::financeiro67_pagarEmDinheiroLancaSangria()
{
    QVERIFY(m_app->criarDespesa(QStringLiteral("Gelo"), QStringLiteral("30,00"), QString()));
    const int id = ultimaContaPagar(m_db.connection());
    const qint64 s = m_app->caixaResumo().value(QStringLiteral("sangrias")).toLongLong();
    QVERIFY(m_app->pagarConta(id, QStringLiteral("dinheiro")));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("sangrias")).toLongLong(), s + 3000);
    QVERIFY(!m_app->pagarConta(id, QStringLiteral("dinheiro")));   // não paga duas vezes
}

void TstVerificacaoGeral::financeiro68_estornoDevolveAGaveta()
{
    QVERIFY(m_app->criarDespesa(QStringLiteral("Errada"), QStringLiteral("12,00"), QString()));
    const int id = ultimaContaPagar(m_db.connection());
    QVERIFY(m_app->pagarConta(id, QStringLiteral("dinheiro")));
    const qint64 a = m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    QVERIFY(m_app->estornarPagamento(id).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(), a + 1200);
}

void TstVerificacaoGeral::financeiro69_despesaPagaNaoSeExclui()
{
    QVERIFY(m_app->criarDespesa(QStringLiteral("Paga"), QStringLiteral("5,00"), QString()));
    const int id = ultimaContaPagar(m_db.connection());
    QVERIFY(m_app->pagarConta(id, QStringLiteral("pix")));
    QVERIFY(!m_app->excluirDespesa(id).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_app->estornarPagamento(id).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_app->excluirDespesa(id).value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::financeiro70_recebimentoParcialDeConta()
{
    const int pid = produto(nomeUnico("Parcial"), {{"Unidade", 1, 2000, {}}});
    const int cli = cliente(nomeUnico("Cli parcial"), 9000);
    QVERIFY(vender({item(pid, emb(pid, 1), 1, 2000)}, {pag("fiado", 2000)}, cli).value(QStringLiteral("ok")).toBool());
    const int conta = static_cast<int>(escalar(QStringLiteral("SELECT MAX(id) FROM contas_receber")));
    QCOMPARE(m_app->receberContaValor(conta, QStringLiteral("7,00"), QStringLiteral("pix"))
                 .value(QStringLiteral("aplicado")).toLongLong(), Q_INT64_C(700));
    QCOMPARE(m_app->cliente(cli).value(QStringLiteral("saldo")).toLongLong(), Q_INT64_C(1300));
}

void TstVerificacaoGeral::financeiro71_contaDeCompraNaoSeExclui()
{
    const int pid = produto(nomeUnico("Conta compra"), {{"Unidade", 1, 500, {}}});
    const QVariantMap r = m_app->registrarCompra(
        {{QStringLiteral("gerarContaPagar"), true},
         {QStringLiteral("itens"), QVariantList{QVariantMap{{QStringLiteral("produtoId"), pid},
                                                            {QStringLiteral("embalagemId"), emb(pid, 1)},
                                                            {QStringLiteral("qtd"), 10},
                                                            {QStringLiteral("custo"), 300}}}}});
    QVERIFY(r.value(QStringLiteral("ok")).toBool());
    const int id = ultimaContaPagar(m_db.connection());
    QCOMPARE(escalar(QStringLiteral("SELECT valor FROM contas_pagar WHERE id = %1").arg(id)), Q_INT64_C(3000));
    QVERIFY(!m_app->excluirDespesa(id).value(QStringLiteral("ok")).toBool());
}

// ============================================================ clientes

// A consulta usava vendas.data_hora (inexistente) e a tela ficava vazia.
void TstVerificacaoGeral::clientes72_historicoDoFiado()
{
    const int pid = produto(nomeUnico("Hist"), {{"Unidade", 1, 500, {}}});
    const int cli = cliente(nomeUnico("Cli hist"), 5000);
    QVERIFY(vender({item(pid, emb(pid, 1), 1, 500)}, {pag("fiado", 500)}, cli).value(QStringLiteral("ok")).toBool());
    const QVariantMap c = m_app->cliente(cli);
    QVERIFY(!c.value(QStringLiteral("ultimaCompraFiado")).toString().isEmpty());
    QCOMPARE(c.value(QStringLiteral("contasAbertas")).toInt(), 1);
    QCOMPARE(c.value(QStringLiteral("limiteDisponivel")).toLongLong(), Q_INT64_C(4500));
}

void TstVerificacaoGeral::clientes73_recebimentoFifo()
{
    const int pid = produto(nomeUnico("Fifo"), {{"Unidade", 1, 500, {}}});
    const int cli = cliente(nomeUnico("Cli fifo"), 9000);
    QVERIFY(vender({item(pid, emb(pid, 1), 2, 500)}, {pag("fiado", 1000)}, cli).value(QStringLiteral("ok")).toBool());
    QVERIFY(vender({item(pid, emb(pid, 1), 1, 500)}, {pag("fiado", 500)}, cli).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_app->receberDeCliente(cli, QStringLiteral("12,00"), QStringLiteral("pix")).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->cliente(cli).value(QStringLiteral("saldo")).toLongLong(), Q_INT64_C(300));
    QCOMPARE(m_app->cliente(cli).value(QStringLiteral("contasAbertas")).toInt(), 1);
}

void TstVerificacaoGeral::clientes74_receberMaisQueADividaLimita()
{
    const int pid = produto(nomeUnico("Limita"), {{"Unidade", 1, 500, {}}});
    const int cli = cliente(nomeUnico("Cli limita"), 9000);
    QVERIFY(vender({item(pid, emb(pid, 1), 1, 500)}, {pag("fiado", 500)}, cli).value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->receberDeCliente(cli, QStringLiteral("50,00"), QStringLiteral("pix"))
                 .value(QStringLiteral("aplicado")).toLongLong(), Q_INT64_C(500));
    QVERIFY(!m_app->receberDeCliente(cli, QString(), QStringLiteral("pix")).value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::clientes75_resumoDoFiado()
{
    const qint64 total = m_app->resumoFiado().value(QStringLiteral("total")).toLongLong();
    QCOMPARE(total, escalar(QStringLiteral("SELECT COALESCE(SUM(valor),0) FROM contas_receber WHERE status='aberta'")));
    QCOMPARE(m_app->dashboard().value(QStringLiteral("aReceber")).toLongLong(), total);
}

// ============================================================ relatórios

void TstVerificacaoGeral::relatorio76_faturamentoDoDia()
{
    const qint64 antes = m_app->relatorioFaturamento(0).value(QStringLiteral("total")).toLongLong();
    const int pid = produto(nomeUnico("Fat"), {{"Unidade", 1, 1250, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 5, QStringLiteral("5,00")));
    QVERIFY(venderSimples(pid, 2, 1250, "pix") > 0);
    const QVariantMap f = m_app->relatorioFaturamento(0);
    QCOMPARE(f.value(QStringLiteral("total")).toLongLong(), antes + 2500);
    QCOMPARE(f.value(QStringLiteral("lucro")).toLongLong(),
             f.value(QStringLiteral("total")).toLongLong() - f.value(QStringLiteral("custo")).toLongLong());
}

// Venda cancelada entrava só com o custo: o "-R$ 7,50" do dashboard.
void TstVerificacaoGeral::relatorio77_canceladaNaoConta()
{
    const QVariantMap antes = m_app->relatorioFaturamento(0);
    const int pid = produto(nomeUnico("Canc rel"), {{"Unidade", 1, 750, {}}});
    QVERIFY(entrar(pid, emb(pid, 1), 5, QStringLiteral("5,00")));
    const int v = venderSimples(pid, 1, 750, "pix");
    QVERIFY(m_app->cancelarVenda(v, QStringLiteral("x")).value(QStringLiteral("ok")).toBool());
    const QVariantMap depois = m_app->relatorioFaturamento(0);
    QCOMPARE(depois.value(QStringLiteral("total")), antes.value(QStringLiteral("total")));
    QCOMPARE(depois.value(QStringLiteral("custo")), antes.value(QStringLiteral("custo")));
}

void TstVerificacaoGeral::relatorio78_diaEspecifico()
{
    const int pid = produto(nomeUnico("Ontem"), {{"Unidade", 1, 3300, {}}});
    const int v = venderSimples(pid, 1, 3300, "pix");
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("UPDATE vendas SET data = datetime(data, '-1 day') WHERE id = %1").arg(v)));
    QVERIFY(q.exec(QStringLiteral("UPDATE movimentacoes_estoque SET data = datetime(data, '-1 day') "
                                  "WHERE origem = 'venda:%1'").arg(v)));
    const QString ontem = QDate::currentDate().addDays(-1).toString(Qt::ISODate);
    QCOMPARE(m_app->relatorioFaturamentoDia(ontem).value(QStringLiteral("total")).toLongLong(), Q_INT64_C(3300));
    QVERIFY(m_app->relatorioFaturamentoDia(QStringLiteral("2026-13-40")).isEmpty());
    QCOMPARE(m_app->relatorioFaturamento(7).value(QStringLiteral("total")).toLongLong()
                 >= m_app->relatorioFaturamento(0).value(QStringLiteral("total")).toLongLong() + 3300, true);
}

// 1800 ml de PARATUDO aparecia acima de cigarros com 250 maços.
void TstVerificacaoGeral::relatorio79_maisVendidosPorEmbalagem()
{
    const int w = produto(QStringLiteral("Zz Whisky rel"), {{"Garrafa", 1000, 9000, {}}}, QStringLiteral("ml"));
    QVERIFY(vender({item(w, emb(w, 1000), 2, 9000, 1000)}, {pag("pix", 18000)}).value(QStringLiteral("ok")).toBool());
    for (const QVariant &v : m_app->relatorioMaisVendidos(0, 50))
        if (v.toMap().value(QStringLiteral("nome")).toString() == QStringLiteral("Zz Whisky rel"))
            QCOMPARE(v.toMap().value(QStringLiteral("qtd")).toLongLong(), Q_INT64_C(2));
}

void TstVerificacaoGeral::relatorio80_dashboardNaoContaDose()
{
    const int garrafa = produto(nomeUnico("Garrafa dash"), {{"Garrafa", 1000, 9000, {}}}, QStringLiteral("ml"));
    QVERIFY(entrar(garrafa, emb(garrafa, 1000), 5, QStringLiteral("50,00")));
    const int antes = m_app->dashboard().value(QStringLiteral("produtosEmFalta")).toInt();
    QVariantMap d = m_app->novoProduto();
    d[QStringLiteral("nome")] = nomeUnico("Dose dash");
    d[QStringLiteral("doseDeProdutoId")] = garrafa;
    d[QStringLiteral("doseQuantidade")] = 50;
    QVERIFY(m_app->salvarProduto(d));
    QCOMPARE(m_app->dashboard().value(QStringLiteral("produtosEmFalta")).toInt(), antes);
}

// Datas em UTC jogavam a venda das 21h para o dia seguinte.
void TstVerificacaoGeral::relatorio81_vendaEmHoraLocal()
{
    const int pid = produto(nomeUnico("Hora"), {{"Unidade", 1, 100, {}}});
    const int v = venderSimples(pid, 1, 100, "pix");
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT data FROM vendas WHERE id = %1").arg(v)) && q.next());
    const QDateTime d = QDateTime::fromString(q.value(0).toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QVERIFY(qAbs(d.secsTo(QDateTime::currentDateTime())) < 120);
}

void TstVerificacaoGeral::relatorio82_cadastroEmHoraLocal()
{
    const QString nome = nomeUnico("Criado");
    const int pid = produto(nome, {{"Unidade", 1, 100, {}}});
    const int cli = cliente(nomeUnico("Cli criado"), 0);
    for (const QString &sql : {QStringLiteral("SELECT criado_em FROM produtos WHERE id = %1").arg(pid),
                               QStringLiteral("SELECT criado_em FROM clientes WHERE id = %1").arg(cli)}) {
        QSqlQuery q(m_db.connection());
        QVERIFY(q.exec(sql) && q.next());
        const QDateTime d = QDateTime::fromString(q.value(0).toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        QVERIFY2(qAbs(d.secsTo(QDateTime::currentDateTime())) < 120, qUtf8Printable(q.value(0).toString()));
    }
}

// ============================================================ copão e dose

void TstVerificacaoGeral::composto83_doseBaixaAGarrafa()
{
    const int garrafa = produto(nomeUnico("Garrafa dose"), {{"Garrafa", 700, 7000, {}}}, QStringLiteral("ml"));
    QVERIFY(entrar(garrafa, emb(garrafa, 700), 1, QStringLiteral("35,00")));
    QVariantMap d = m_app->novoProduto();
    const QString nome = nomeUnico("Dose");
    d[QStringLiteral("nome")] = nome;
    d[QStringLiteral("doseDeProdutoId")] = garrafa;
    d[QStringLiteral("doseQuantidade")] = 50;
    QVariantMap e = d.value(QStringLiteral("embalagens")).toList().first().toMap();
    e[QStringLiteral("preco")] = 800;
    d[QStringLiteral("embalagens")] = QVariantList{e};
    QVERIFY(m_app->salvarProduto(d));
    const int dose = m_app->buscarProdutosPorNome(nome).first().toMap().value(QStringLiteral("produtoId")).toInt();
    QVERIFY(vender({item(dose, emb(dose, 1), 2, 800)}, {pag("pix", 1600)}).value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(garrafa), Q_INT64_C(600));
    QCOMPARE(escalar(QStringLiteral("SELECT COUNT(*) FROM estoque WHERE produto_id = %1").arg(dose)), Q_INT64_C(0));
}

void TstVerificacaoGeral::composto84_doseDisponivelPelaGarrafa()
{
    const int garrafa = produto(nomeUnico("Garrafa disp"), {{"Garrafa", 700, 7000, {}}}, QStringLiteral("ml"));
    QVERIFY(entrar(garrafa, emb(garrafa, 700), 1, QStringLiteral("35,00")));
    QVariantMap d = m_app->novoProduto();
    const QString nome = nomeUnico("Dose disp");
    d[QStringLiteral("nome")] = nome;
    d[QStringLiteral("doseDeProdutoId")] = garrafa;
    d[QStringLiteral("doseQuantidade")] = 50;
    QVERIFY(m_app->salvarProduto(d));
    const int dose = m_app->buscarProdutosPorNome(nome).first().toMap().value(QStringLiteral("produtoId")).toInt();
    QCOMPARE(m_app->estoqueDisponivel(dose), 14LL);
}

static QVariantMap linhaComposicao(int cat, const QString &un, int qtd, int padrao)
{
    return {{QStringLiteral("categoriaId"), cat}, {QStringLiteral("unidade"), un},
            {QStringLiteral("quantidade"), qtd}, {QStringLiteral("produtoPadraoId"), padrao},
            {QStringLiteral("travada"), false}};
}

void TstVerificacaoGeral::composto85_copaoBaixaInsumos()
{
    const int cat = m_app->criarCategoria(QStringLiteral("Verif Destilados"));
    QVERIFY(cat > 0);
    QVariantMap g = m_app->novoProduto();
    g[QStringLiteral("nome")] = QStringLiteral("Verif Gin");
    g[QStringLiteral("categoriaId")] = cat;
    g[QStringLiteral("unidadeBase")] = QStringLiteral("ml");
    g[QStringLiteral("embalagens")] = QVariantList{QVariantMap{{QStringLiteral("id"), 0}, {QStringLiteral("nome"), QStringLiteral("Garrafa")},
                                                               {QStringLiteral("fator"), 1000}, {QStringLiteral("preco"), 10000}}};
    QVERIFY(m_app->salvarProduto(g));
    const int gin = m_app->buscarProdutosPorNome(QStringLiteral("Verif Gin")).first().toMap().value(QStringLiteral("produtoId")).toInt();

    QVariantMap c = m_app->novoProduto();
    c[QStringLiteral("nome")] = QStringLiteral("Verif Copao");
    c[QStringLiteral("composto")] = true;
    c[QStringLiteral("composicao")] = QVariantList{linhaComposicao(cat, QStringLiteral("ml"), 80, gin)};
    QVariantMap ec = c.value(QStringLiteral("embalagens")).toList().first().toMap();
    ec[QStringLiteral("preco")] = 2500;
    c[QStringLiteral("embalagens")] = QVariantList{ec};
    QVERIFY(m_app->salvarProduto(c));
    const int copao = m_app->buscarProdutosPorNome(QStringLiteral("Verif Copao")).first().toMap().value(QStringLiteral("produtoId")).toInt();

    QVariantMap it = item(copao, 0, 2, 2500);
    it[QStringLiteral("insumos")] = QVariantList{QVariantMap{{QStringLiteral("produtoId"), gin}, {QStringLiteral("quantidade"), 80}}};
    QVERIFY(vender({it}, {pag("pix", 5000)}).value(QStringLiteral("ok")).toBool());
    QCOMPARE(saldo(gin), Q_INT64_C(-160));
    QCOMPARE(escalar(QStringLiteral("SELECT COUNT(*) FROM movimentacoes_estoque WHERE produto_id = %1").arg(copao)),
             Q_INT64_C(0));

    // Composto sem os insumos escolhidos não passa.
    QVERIFY(!vender({item(copao, 0, 1, 2500)}, {pag("pix", 2500)}).value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::composto86_funcionarioNaoMudaPrecoDoCopao()
{
    const int copao = m_app->buscarProdutosPorNome(QStringLiteral("Verif Copao")).first().toMap().value(QStringLiteral("produtoId")).toInt();
    const int gin = m_app->buscarProdutosPorNome(QStringLiteral("Verif Gin")).first().toMap().value(QStringLiteral("produtoId")).toInt();
    comoFuncionario();
    QVariantMap it = item(copao, 0, 1, 1500);   // cadastrado a 25,00
    it[QStringLiteral("insumos")] = QVariantList{QVariantMap{{QStringLiteral("produtoId"), gin}, {QStringLiteral("quantidade"), 80}}};
    const QVariantMap r = vender({it}, {pag("pix", 1500)});
    QVERIFY(!r.value(QStringLiteral("ok")).toBool());
    QVERIFY(r.value(QStringLiteral("erro")).toString().contains(QStringLiteral("preço")));
}

// ============================================================ validade

static qint64 emLotes(const QSqlDatabase &db, int pid)
{
    QSqlQuery q(db);
    return (q.exec(QStringLiteral("SELECT COALESCE(SUM(quantidade),0) FROM lotes WHERE produto_id = %1").arg(pid))
            && q.next()) ? q.value(0).toLongLong() : -1;
}

void TstVerificacaoGeral::validade87_entradaComValidadeCriaLote()
{
    const int pid = produto(QStringLiteral("Zz Doce validade"), {{"Unidade", 1, 300, {}}});
    const QString validade = QDate::currentDate().addDays(20).toString(Qt::ISODate);
    QVERIFY(m_app->registrarEntrada(pid, emb(pid, 1), 10, QStringLiteral("1,00"), QString(), validade,
                                    QStringLiteral("L1")));
    QCOMPARE(emLotes(m_db.connection(), pid), Q_INT64_C(10));
    QVERIFY(m_app->resumoVencimento().value(QStringLiteral("venceEm30")).toInt() >= 1);
}

void TstVerificacaoGeral::validade88_vendaConsomeLote()
{
    const int pid = m_app->buscarProdutosPorNome(QStringLiteral("Zz Doce validade")).first().toMap().value(QStringLiteral("produtoId")).toInt();
    QVERIFY(venderSimples(pid, 3, 300, "pix") > 0);
    QCOMPARE(emLotes(m_db.connection(), pid), Q_INT64_C(7));
}

void TstVerificacaoGeral::validade89_retiradaConsomeLote()
{
    const int pid = m_app->buscarProdutosPorNome(QStringLiteral("Zz Doce validade")).first().toMap().value(QStringLiteral("produtoId")).toInt();
    QVERIFY(m_app->registrarRetirada(pid, emb(pid, 1), 2, QStringLiteral("vencido")));
    QCOMPARE(emLotes(m_db.connection(), pid), Q_INT64_C(5));
    QCOMPARE(saldo(pid), Q_INT64_C(5));
}

// ============================================================ backup

void TstVerificacaoGeral::backup90_fazerBackupCriaArquivo()
{
    const QVariantMap r = m_app->fazerBackup();
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(), qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    QVERIFY(QFileInfo::exists(r.value(QStringLiteral("caminho")).toString()));
    QVERIFY(m_app->conferirArquivoBackup(r.value(QStringLiteral("caminho")).toString()).value(QStringLiteral("ok")).toBool());
}

// Escolher o relatorio.html por engano trocaria o banco por lixo.
void TstVerificacaoGeral::backup91_recusaArquivoQueNaoEBackup()
{
    const QString falso = m_dir.filePath(QStringLiteral("relatorio.html"));
    QFile f(falso);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(8192, 'x'));
    f.close();
    QVERIFY(!m_app->conferirArquivoBackup(falso).value(QStringLiteral("ok")).toBool());
    QVERIFY(!m_app->agendarRestauracao(falso).value(QStringLiteral("ok")).toBool());
}

void TstVerificacaoGeral::backup92_listaOBackupFeito()
{
    QVERIFY(!m_app->backupsDisponiveis().isEmpty());
    QVERIFY(m_app->statusBackup().value(QStringLiteral("total")).toInt() <= 5);   // retenção
}

void TstVerificacaoGeral::backup93_funcionarioNaoFazBackup()
{
    comoFuncionario();
    QVERIFY(!m_app->fazerBackup().value(QStringLiteral("ok")).toBool());
    QVERIFY(!m_app->agendarRestauracao(QStringLiteral("x.db")).value(QStringLiteral("ok")).toBool());
}

// ============================================================ fotos

void TstVerificacaoGeral::foto94_gravaReduzida()
{
    const int pid = produto(nomeUnico("Foto"), {{"Unidade", 1, 100, {}}});
    QImage img(1200, 900, QImage::Format_RGB32);
    img.fill(Qt::darkYellow);
    const QString arq = m_dir.filePath(QStringLiteral("foto.png"));
    QVERIFY(img.save(arq));
    const QVariantMap r = m_app->definirFotoProduto(pid, arq);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(), qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    const QImage gravada = QImage::fromData(ProdutoRepository(m_db.connection()).foto(pid));
    QCOMPARE(qMax(gravada.width(), gravada.height()), 320);
    QVERIFY(m_app->produtoTemFoto(pid));
    QVERIFY(m_app->removerFotoProduto(pid));
    QVERIFY(!m_app->produtoTemFoto(pid));
}

void TstVerificacaoGeral::foto95_heicComOrientacao()
{
    const int pid = produto(nomeUnico("Heic"), {{"Unidade", 1, 100, {}}});
    const QString arq = m_dir.filePath(QStringLiteral("IMG_0001.HEIC"));
    QFile f(arq);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("nao e imagem");
    f.close();
    const QVariantMap r = m_app->definirFotoProduto(pid, arq);
    QVERIFY(!r.value(QStringLiteral("ok")).toBool());
    QVERIFY(r.value(QStringLiteral("erro")).toString().contains(QStringLiteral("HEIC")));
}

// ============================================================ registro

void TstVerificacaoGeral::log96_cancelamentoFicaNoRegistro()
{
    const int pid = produto(nomeUnico("Log"), {{"Unidade", 1, 100, {}}});
    const int v = venderSimples(pid, 1, 100, "pix");
    QVERIFY(m_app->cancelarVenda(v, QStringLiteral("motivo do log")).value(QStringLiteral("ok")).toBool());
    const QString log = m_app->ultimasLinhasLog(50).join(QLatin1Char('\n'));
    QVERIFY2(log.contains(QStringLiteral("Venda #%1 CANCELADA").arg(v)), qUtf8Printable(log.left(400)));
    QVERIFY(log.contains(QStringLiteral("motivo do log")));
}

QTEST_MAIN(TstVerificacaoGeral)
#include "tst_verificacao_geral.moc"
