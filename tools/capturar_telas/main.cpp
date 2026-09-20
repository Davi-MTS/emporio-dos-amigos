// =============================================================================
// tools/capturar_telas — gera as imagens do manual a partir das telas DE VERDADE
// =============================================================================
// O manual do dono precisa mostrar o sistema como ele é. Desenhar as telas à mão
// garante que um dia elas deixem de bater com o programa e o manual passe a
// ensinar errado. Aqui as imagens saem do mesmo Main.qml que roda na loja, sobre
// um banco temporário com dados de exemplo.
//
// Rodar:
//   set QT_QPA_PLATFORM=offscreen & set QT_QUICK_BACKEND=software
//   capturar_telas.exe <pasta-de-saida>
//
// Nada aqui toca no banco da loja: QStandardPaths entra em modo de teste e o
// banco vive num QTemporaryDir.
// =============================================================================

#include <QDate>
#include <QDir>
#include <QFontDatabase>
#include <QQuickItem>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <QVector>

#include "app/AppBackend.h"
#include "models/EstoqueListModel.h"
#include "app/ProdutoFotoProvider.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"

namespace {

// Deixa a interface assentar: a troca de página tem transição em fade e as
// listas carregam em outra volta do laço de eventos. Sem esta pausa a captura
// sai no meio da animação.
void assentar(int ms = 450)
{
    QEventLoop laco;
    QTimer::singleShot(ms, &laco, &QEventLoop::quit);
    laco.exec();
}

bool capturar(QQuickWindow *janela, const QString &pasta, const QString &nome)
{
    assentar();
    const QImage img = janela->grabWindow();
    if (img.isNull()) {
        qWarning("nao consegui capturar %s", qPrintable(nome));
        return false;
    }
    const QString caminho = pasta + QLatin1Char('/') + nome + QStringLiteral(".png");
    if (!img.save(caminho, "PNG")) {
        qWarning("nao consegui gravar %s", qPrintable(caminho));
        return false;
    }
    qInfo("  %s  (%dx%d)", qPrintable(nome), img.width(), img.height());
    return true;
}

// Acha a tela carregada no Loader de conteudo. Os tipos QML ganham nomes de
// classe como "PdvScreen_QMLTYPE_42", entao a busca e por prefixo.
QQuickItem *acharTela(QQuickWindow *janela, const QString &prefixo)
{
    const auto itens = janela->findChildren<QQuickItem *>();
    for (QQuickItem *i : itens) {
        if (QString::fromLatin1(i->metaObject()->className()).startsWith(prefixo))
            return i;
    }
    return nullptr;
}

int criarProduto(AppBackend &app, ProdutoRepository &repo, const QString &nome,
                 int categoriaId, const QString &unidade, const QString &embalagem,
                 int fator, qint64 preco, const QString &codigo, int minimo)
{
    QVariantMap p = app.novoProduto();
    p[QStringLiteral("nome")] = nome;
    p[QStringLiteral("categoriaId")] = categoriaId;
    p[QStringLiteral("unidadeBase")] = unidade;
    p[QStringLiteral("estoqueMinimo")] = minimo;
    QVariantMap e;
    e[QStringLiteral("id")] = 0;
    e[QStringLiteral("nome")] = embalagem;
    e[QStringLiteral("fator")] = fator;
    e[QStringLiteral("codigoBarras")] = codigo;
    e[QStringLiteral("preco")] = static_cast<qlonglong>(preco);
    e[QStringLiteral("custo")] = -1;
    p[QStringLiteral("embalagens")] = QVariantList{e};
    if (!app.salvarProduto(p)) {
        qWarning("produto %s: %s", qPrintable(nome), qPrintable(app.ultimoErro()));
        return 0;
    }
    for (const Produto &pr : repo.listar(nome))
        if (pr.nome == nome)
            return pr.id;
    return 0;
}

int idCategoria(AppBackend &app, const QString &nome)
{
    for (const QVariant &v : app.categorias()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("nome")).toString().compare(nome, Qt::CaseInsensitive) == 0)
            return m.value(QStringLiteral("id")).toInt();
    }
    return app.categorias().isEmpty()
               ? 0
               : app.categorias().first().toMap().value(QStringLiteral("id")).toInt();
}

// Dados de exemplo. Telas vazias não ensinam nada: o manual precisa mostrar
// lista com produto, caixa aberto, venda feita, cliente devendo e conta a pagar.
void semear(AppBackend &app, QSqlDatabase db)
{
    ProdutoRepository repo(db);

    const int cerveja = idCategoria(app, QStringLiteral("Cervejas"));
    const int destilado = idCategoria(app, QStringLiteral("Destilados"));
    const int energetico = idCategoria(app, QStringLiteral("Energéticos"));

    const int heineken = criarProduto(app, repo, QStringLiteral("Heineken Long Neck"),
                                      cerveja, QStringLiteral("unidade"),
                                      QStringLiteral("Unidade"), 1, 890,
                                      QStringLiteral("7896045506873"), 24);
    const int skol = criarProduto(app, repo, QStringLiteral("Skol Lata 350ml"),
                                  cerveja, QStringLiteral("unidade"),
                                  QStringLiteral("Unidade"), 1, 450,
                                  QStringLiteral("7891991010023"), 48);
    const int gin = criarProduto(app, repo, QStringLiteral("Gin Beefeater 750ml"),
                                 destilado, QStringLiteral("ml"),
                                 QStringLiteral("Garrafa 750 ml"), 750, 12900,
                                 QStringLiteral("5000299602058"), 1500);
    const int monster = criarProduto(app, repo, QStringLiteral("Monster Energy 473ml"),
                                     energetico, QStringLiteral("unidade"),
                                     QStringLiteral("Unidade"), 1, 1200,
                                     QStringLiteral("7898934500115"), 24);
    // Nunca recebeu entrada: aparece como "Zerado" no filtro do Estoque.
    criarProduto(app, repo, QStringLiteral("Red Bull 250ml"),
                 energetico, QStringLiteral("unidade"),
                 QStringLiteral("Unidade"), 1, 1100,
                 QStringLiteral("9002490100070"), 12);

    // Entradas com custo, para o lucro e o valor imobilizado não saírem zerados.
    app.registrarEntrada(heineken, app.embalagensDe(heineken).first().toMap()
                                       .value(QStringLiteral("id")).toInt(),
                         120, QStringLiteral("5,20"), QStringLiteral("Carga da semana"),
                         QString(), QString());
    app.registrarEntrada(skol, app.embalagensDe(skol).first().toMap()
                                   .value(QStringLiteral("id")).toInt(),
                         240, QStringLiteral("2,60"), QString(), QString(), QString());
    app.registrarEntrada(gin, app.embalagensDe(gin).first().toMap()
                                  .value(QStringLiteral("id")).toInt(),
                         6, QStringLiteral("74,00"), QString(), QString(), QString());
    app.registrarEntrada(monster, app.embalagensDe(monster).first().toMap()
                                      .value(QStringLiteral("id")).toInt(),
                         18, QStringLiteral("7,40"),
                         QString(), QStringLiteral("2026-10-15"), QStringLiteral("L-2210"));

    // Cliente com fiado em aberto.
    QVariantMap cli;
    cli[QStringLiteral("id")] = 0;
    cli[QStringLiteral("nome")] = QStringLiteral("Marcos Vinícius");
    cli[QStringLiteral("telefone")] = QStringLiteral("(62) 99123-4567");
    cli[QStringLiteral("limite")] = 20000;   // centavos, como o backend espera
    app.salvarCliente(cli);
    int clienteId = 0;
    for (const QVariant &v : app.clientesLista())
        clienteId = v.toMap().value(QStringLiteral("id")).toInt();

    // Fornecedor.
    QVariantMap forn;
    forn[QStringLiteral("id")] = 0;
    forn[QStringLiteral("nome")] = QStringLiteral("Distribuidora Central");
    forn[QStringLiteral("telefone")] = QStringLiteral("(62) 3333-1010");
    app.salvarFornecedor(forn);

    // Caixa aberto e algumas vendas, com formas diferentes.
    app.abrirCaixa(QStringLiteral("150,00"));

    auto vender = [&](int produtoId, int qtd, const QString &forma, qint64 valor) {
        const QVariantMap emb = app.embalagensDe(produtoId).first().toMap();
        QVariantMap item;
        item[QStringLiteral("produtoId")] = produtoId;
        item[QStringLiteral("embalagemId")] = emb.value(QStringLiteral("id")).toInt();
        item[QStringLiteral("fator")] = emb.value(QStringLiteral("fator")).toInt();
        item[QStringLiteral("qtd")] = qtd;
        item[QStringLiteral("precoUnit")] = emb.value(QStringLiteral("preco"));
        item[QStringLiteral("desconto")] = 0;

        QVariantMap pag;
        pag[QStringLiteral("forma")] = forma;
        pag[QStringLiteral("valor")] = static_cast<qlonglong>(valor);

        QVariantMap venda;
        venda[QStringLiteral("itens")] = QVariantList{item};
        venda[QStringLiteral("pagamentos")] = QVariantList{pag};
        venda[QStringLiteral("desconto")] = 0;
        const QVariantMap r = app.finalizarVenda(venda);
        if (!r.value(QStringLiteral("ok")).toBool())
            qWarning("venda: %s", qPrintable(r.value(QStringLiteral("erro")).toString()));
    };

    vender(heineken, 6, QStringLiteral("dinheiro"), 6000);
    vender(skol, 12, QStringLiteral("pix"), 5400);
    vender(monster, 2, QStringLiteral("debito"), 2400);
    vender(heineken, 4, QStringLiteral("credito"), 3560);

    // Uma venda no fiado, para a tela de Clientes ter saldo devedor.
    if (clienteId > 0) {
        const QVariantMap emb = app.embalagensDe(skol).first().toMap();
        QVariantMap item;
        item[QStringLiteral("produtoId")] = skol;
        item[QStringLiteral("embalagemId")] = emb.value(QStringLiteral("id")).toInt();
        item[QStringLiteral("fator")] = emb.value(QStringLiteral("fator")).toInt();
        item[QStringLiteral("qtd")] = 10;
        item[QStringLiteral("precoUnit")] = emb.value(QStringLiteral("preco"));
        item[QStringLiteral("desconto")] = 0;
        // Fiado e uma FORMA DE PAGAMENTO, com o cliente na venda -- nao uma
        // venda sem pagamento nenhum.
        QVariantMap pag;
        pag[QStringLiteral("forma")] = QStringLiteral("fiado");
        pag[QStringLiteral("valor")] = static_cast<qlonglong>(4500);

        QVariantMap venda;
        venda[QStringLiteral("itens")] = QVariantList{item};
        venda[QStringLiteral("pagamentos")] = QVariantList{pag};
        venda[QStringLiteral("desconto")] = 0;
        venda[QStringLiteral("clienteId")] = clienteId;
        const QVariantMap r = app.finalizarVenda(venda);
        if (!r.value(QStringLiteral("ok")).toBool())
            qWarning("venda no fiado: %s",
                     qPrintable(r.value(QStringLiteral("erro")).toString()));
    }

    // Contas a pagar: uma vencendo e uma vencida, para a tela mostrar os dois casos.
    app.criarDespesa(QStringLiteral("Energia elétrica"), QStringLiteral("380,00"),
                     QDate::currentDate().addDays(5).toString(Qt::ISODate));
    app.criarDespesa(QStringLiteral("Aluguel do ponto"), QStringLiteral("1.800,00"),
                     QDate::currentDate().addDays(-2).toString(Qt::ISODate));

    app.recarregarProdutos();
    app.recarregarEstoque();
    app.recarregarFinanceiro();
    app.recarregarVendas(30);
}

// ---------------------------------------------------------------------------
// ROTEIRO DA AUDITORIA — uma imagem por correção, tirada da tela de verdade.
//
// Não é "print bonito": cada cena é montada chamando as mesmas funções que a
// tela chama quando o operador clica, para a imagem provar que a correção está
// no programa. O relatório em docs/relatorio-auditoria.html usa estas imagens.
// ---------------------------------------------------------------------------

// Todos os itens VISÍVEIS com este objectName.
//
// A busca é pela árvore VISUAL (childItems), não por findChildren: item criado
// por Repeater ou Instantiator costuma não ter pai QObject, e findChildren não
// enxerga nenhum deles — as linhas de embalagem do cadastro e as linhas de item
// da compra são exatamente assim. A árvore visual também alcança os diálogos,
// que vivem no overlay da janela.
void juntarPorNome(QQuickItem *raiz, const QString &nome, QVector<QQuickItem *> *achados)
{
    if (!raiz)
        return;
    if (raiz->objectName() == nome && raiz->isVisible())
        achados->push_back(raiz);
    const auto filhos = raiz->childItems();
    for (QQuickItem *f : filhos)
        juntarPorNome(f, nome, achados);
}

QVector<QQuickItem *> acharTodos(QQuickWindow *janela, const QString &nome)
{
    QVector<QQuickItem *> achados;
    juntarPorNome(janela->contentItem(), nome, &achados);
    return achados;
}

QQuickItem *achar(QQuickWindow *janela, const QString &nome)
{
    const auto todos = acharTodos(janela, nome);
    if (todos.isEmpty()) {
        // Sem isto a cena sai "quase certa" e ninguém percebe que o clique não
        // aconteceu — foi o que houve na primeira rodada das capturas.
        qWarning("  !! nao achei o item visivel \"%s\"", qPrintable(nome));
        return nullptr;
    }
    return todos.last();
}

QObject *acharObjeto(QQuickWindow *janela, const QString &nome)
{
    QObject *o = janela->findChild<QObject *>(nome);
    if (!o)
        qWarning("  !! nao achei o objeto \"%s\"", qPrintable(nome));
    return o;
}

void clicar(QQuickItem *botao)
{
    if (botao)
        QMetaObject::invokeMethod(botao, "clicked");
    assentar(120);
}

void escrever(QQuickItem *campo, const QString &texto)
{
    if (campo)
        campo->setProperty("text", texto);
    assentar(120);
}

void capturarAuditoria(QQuickWindow *janela, AppBackend &backend, QSqlDatabase db,
                       const QString &pasta)
{
    ProdutoRepository repo(db);
    qInfo("Capturando as correções da auditoria");

    // Produto com caixinha de 12: é o caso do IMPERIO na loja.
    const int cerveja = idCategoria(backend, QStringLiteral("Cervejas"));
    int brahma = 0;
    {
        QVariantMap p = backend.novoProduto();
        p[QStringLiteral("nome")] = QStringLiteral("Brahma Chopp 350ml");
        p[QStringLiteral("categoriaId")] = cerveja;
        QVariantMap un{{QStringLiteral("id"), 0}, {QStringLiteral("nome"), QStringLiteral("Unidade")},
                       {QStringLiteral("fator"), 1}, {QStringLiteral("codigoBarras"), QStringLiteral("7891149101023")},
                       {QStringLiteral("preco"), 450}, {QStringLiteral("custo"), -1}};
        QVariantMap cx{{QStringLiteral("id"), 0}, {QStringLiteral("nome"), QStringLiteral("Caixinha")},
                       {QStringLiteral("fator"), 12}, {QStringLiteral("codigoBarras"), QString()},
                       {QStringLiteral("preco"), 5200}, {QStringLiteral("custo"), -1}};
        p[QStringLiteral("embalagens")] = QVariantList{un, cx};
        if (!backend.salvarProduto(p))
            qWarning("brahma: %s", qPrintable(backend.ultimoErro()));
        for (const Produto &pr : repo.listar(QStringLiteral("Brahma Chopp 350ml")))
            brahma = pr.id;
    }
    const int embUnidade = backend.embalagensDe(brahma).first().toMap().value(QStringLiteral("id")).toInt();
    int embCaixinha = 0;
    for (const QVariant &v : backend.embalagensDe(brahma))
        if (v.toMap().value(QStringLiteral("fator")).toInt() == 12)
            embCaixinha = v.toMap().value(QStringLiteral("id")).toInt();
    backend.registrarEntrada(brahma, embUnidade, 48, QStringLiteral("3,00"), QString(),
                             QString(), QString());

    // ---- 1 e 2) Cadastro: fator obrigatório e fator ambíguo -----------------
    QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, QStringLiteral("produtos")));
    assentar(500);
    QQuickItem *produtos = acharTela(janela, QStringLiteral("ProdutosScreen"));
    qInfo("  produtos=%p brahma=%d embCaixinha=%d", (void *)produtos, brahma, embCaixinha);
    if (produtos) {
        QMetaObject::invokeMethod(produtos, "abrirProduto", Q_ARG(QVariant, brahma));
        assentar(300);
        if (QObject *abas = acharObjeto(janela, QStringLiteral("abasProduto")))
            abas->setProperty("currentIndex", 1);
        assentar(300);
        clicar(achar(janela, QStringLiteral("adicionarEmbalagem")));
        escrever(achar(janela, QStringLiteral("nomeEmbalagem")), QStringLiteral("FARDO"));
        escrever(achar(janela, QStringLiteral("precoEmbalagem")), QStringLiteral("29,00"));
        clicar(achar(janela, QStringLiteral("salvarProduto")));
        capturar(janela, pasta, QStringLiteral("fix-01-fator-obrigatorio"));

        // Agora com fator 1 (o erro da loja): mesma quantidade, dois preços.
        // O valueModified é o sinal que a tela escuta para gravar no model —
        // mudar só a propriedade "value" muda o número na tela e mais nada.
        if (QQuickItem *fator = achar(janela, QStringLiteral("fatorEmbalagem"))) {
            fator->setProperty("value", 1);
            QMetaObject::invokeMethod(fator, "valueModified");
        }
        assentar(150);
        clicar(achar(janela, QStringLiteral("salvarProduto")));
        capturar(janela, pasta, QStringLiteral("fix-02-fator-ambiguo"));
        QMetaObject::invokeMethod(produtos, "fecharEditor");
        assentar(200);
    }

    // ---- 3) Compra: aviso de custo fora do normal ---------------------------
    QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, QStringLiteral("compras")));
    assentar(500);
    QObject *novaCompra = acharObjeto(janela, QStringLiteral("novaCompraDialog"));
    if (novaCompra) {
        QMetaObject::invokeMethod(novaCompra, "abrir");
        assentar(300);
        QVariantMap item{{QStringLiteral("produtoId"), brahma},
                         {QStringLiteral("nome"), QStringLiteral("Brahma Chopp 350ml")},
                         {QStringLiteral("embalagemId"), embCaixinha},
                         {QStringLiteral("fator"), 12}};
        QMetaObject::invokeMethod(novaCompra, "adicionarProduto", Q_ARG(QVariant, item));
        assentar(250);
        escrever(achar(janela, QStringLiteral("custoItemCompra")), QStringLiteral("3,00"));
        clicar(achar(janela, QStringLiteral("registrarCompra")));
        capturar(janela, pasta, QStringLiteral("fix-03-aviso-custo-compra"));

        // ---- 4) Custo sugerido exato em produto de ml ----------------------
        int gin = 0;
        for (const Produto &pr : repo.listar(QStringLiteral("Gin Beefeater 750ml")))
            gin = pr.id;
        if (gin > 0) {
            const QVariantMap embGin = backend.embalagensDe(gin).first().toMap();
            QVariantMap itemGin{{QStringLiteral("produtoId"), gin},
                                {QStringLiteral("nome"), QStringLiteral("Gin Beefeater 750ml")},
                                {QStringLiteral("embalagemId"), embGin.value(QStringLiteral("id")).toInt()},
                                {QStringLiteral("fator"), embGin.value(QStringLiteral("fator")).toInt()}};
            QMetaObject::invokeMethod(novaCompra, "adicionarProduto", Q_ARG(QVariant, itemGin));
            assentar(250);
            capturar(janela, pasta, QStringLiteral("fix-04-custo-sugerido-ml"));
        }
        QMetaObject::invokeMethod(novaCompra, "close");
        assentar(200);
    }

    // ---- 5) Entrada de estoque: aviso de custo alto -------------------------
    QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, QStringLiteral("estoque")));
    assentar(500);
    if (QQuickItem *estoque = acharTela(janela, QStringLiteral("EstoqueScreen"))) {
        QMetaObject::invokeMethod(estoque, "abrirMov", Q_ARG(QVariant, brahma));
        assentar(350);
        escrever(achar(janela, QStringLiteral("custoEntrada")), QStringLiteral("52,00"));
        clicar(achar(janela, QStringLiteral("confirmarMovimento")));
        capturar(janela, pasta, QStringLiteral("fix-05-aviso-custo-entrada"));
        if (QObject *dlg = acharObjeto(janela, QStringLiteral("movDialog")))
            QMetaObject::invokeMethod(dlg, "close");
        assentar(200);
    }

    // ---- 6 e 7) Clientes: histórico do fiado e a confirmação de desativar ---
    int clienteId = 0;
    for (const QVariant &v : backend.clientesLista())
        clienteId = v.toMap().value(QStringLiteral("id")).toInt();
    QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, QStringLiteral("clientes")));
    assentar(500);
    if (QQuickItem *clientes = acharTela(janela, QStringLiteral("ClientesScreen"))) {
        QMetaObject::invokeMethod(clientes, "abrirCliente", Q_ARG(QVariant, clienteId));
        assentar(350);
        capturar(janela, pasta, QStringLiteral("fix-06-historico-fiado"));
        clicar(achar(janela, QStringLiteral("desativarCliente")));
        capturar(janela, pasta, QStringLiteral("fix-07-desativar-cliente"));
        if (QObject *dlg = acharObjeto(janela, QStringLiteral("confirmarDesativarCliente")))
            QMetaObject::invokeMethod(dlg, "close");
        assentar(200);
    }

    // ---- 8) Cancelar venda cujo fiado já foi pago: sai da gaveta ------------
    int vendaFiado = 0;
    {
        // Venda no fiado, recebida em dinheiro, e depois cancelada na tela.
        QVariantMap item{{QStringLiteral("produtoId"), brahma},
                         {QStringLiteral("embalagemId"), embUnidade},
                         {QStringLiteral("fator"), 1}, {QStringLiteral("qtd"), 4},
                         {QStringLiteral("precoUnit"), 450}, {QStringLiteral("desconto"), 0}};
        QVariantMap venda{{QStringLiteral("itens"), QVariantList{item}},
                          {QStringLiteral("pagamentos"), QVariantList{QVariantMap{
                               {QStringLiteral("forma"), QStringLiteral("fiado")},
                               {QStringLiteral("valor"), 1800}}}},
                          {QStringLiteral("desconto"), 0},
                          {QStringLiteral("clienteId"), clienteId}};
        const QVariantMap r = backend.finalizarVenda(venda);
        vendaFiado = r.value(QStringLiteral("vendaId")).toInt();
        // Quita TUDO: o recebimento abate da conta mais antiga (FIFO), então
        // pagar só os 18,00 quitaria a dívida anterior e esta venda seguiria
        // em aberto — sem o caso que a imagem precisa mostrar.
        backend.receberDeCliente(clienteId, QString(), QStringLiteral("dinheiro"));
        backend.recarregarVendas(30);
    }
    QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, QStringLiteral("vendas")));
    assentar(500);
    if (QObject *detalhe = acharObjeto(janela, QStringLiteral("detalheVenda"))) {
        QMetaObject::invokeMethod(detalhe, "abrir", Q_ARG(QVariant, vendaFiado),
                                  Q_ARG(QVariant, 1800), Q_ARG(QVariant, false));
        assentar(300);
        escrever(achar(janela, QStringLiteral("motivoCancelamento")),
                 QStringLiteral("cliente devolveu a mercadoria"));
        clicar(achar(janela, QStringLiteral("confirmarCancelamento")));
        capturar(janela, pasta, QStringLiteral("fix-08-cancelar-fiado-pago"));
        QMetaObject::invokeMethod(detalhe, "close");
        assentar(200);
    }

    // ---- 9) Mais vendidos: ml contado por garrafa, não por mililitro --------
    {
        int gin = 0;
        for (const Produto &pr : repo.listar(QStringLiteral("Gin Beefeater 750ml")))
            gin = pr.id;
        const QVariantMap embGin = backend.embalagensDe(gin).first().toMap();
        QVariantMap item{{QStringLiteral("produtoId"), gin},
                         {QStringLiteral("embalagemId"), embGin.value(QStringLiteral("id")).toInt()},
                         {QStringLiteral("fator"), embGin.value(QStringLiteral("fator")).toInt()},
                         {QStringLiteral("qtd"), 2}, {QStringLiteral("precoUnit"), 12900},
                         {QStringLiteral("desconto"), 0}};
        QVariantMap venda{{QStringLiteral("itens"), QVariantList{item}},
                          {QStringLiteral("pagamentos"), QVariantList{QVariantMap{
                               {QStringLiteral("forma"), QStringLiteral("pix")},
                               {QStringLiteral("valor"), 25800}}}},
                          {QStringLiteral("desconto"), 0}};
        backend.finalizarVenda(venda);
    }
    QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, QStringLiteral("relatorios")));
    assentar(600);
    capturar(janela, pasta, QStringLiteral("fix-09-mais-vendidos"));
}

}  // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    const QString pasta = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                   : QStringLiteral("capturas");
    QDir().mkpath(pasta);

    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("DistribuidoraManual"));
    QCoreApplication::setApplicationName(QStringLiteral("DistribuidoraManual"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // As MESMAS fontes que o main.cpp carrega. Sem isto a captura sai com o
    // texto todo em quadradinhos: a plataforma offscreen nao tem fonte nenhuma
    // do sistema, e o manual ficaria ilegivel.
    if (QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-Variable.ttf")) < 0
        || QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Fraunces-Variable.ttf")) < 0) {
        qWarning("as fontes embutidas nao carregaram - a captura sairia ilegivel");
        return 1;
    }
    QGuiApplication::setFont(QFont(QStringLiteral("Archivo")));

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        qWarning("sem pasta temporaria");
        return 1;
    }

    Database db;
    if (!db.open(tmp.filePath(QStringLiteral("manual.db")))) {
        qWarning("banco: %s", qPrintable(db.lastError()));
        return 1;
    }
    MigrationRunner runner(db.connection());
    if (!runner.migrate() || !runner.seed()) {
        qWarning("migrations: %s", qPrintable(runner.lastError()));
        return 1;
    }

    AppBackend backend(db.connection());
    backend.criarAdmin(QStringLiteral("Davi"), QStringLiteral("davi"),
                       QStringLiteral("davi12345"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &backend);
    engine.addImageProvider(QString::fromLatin1(ProdutoFotoProvider::nome()),
                            new ProdutoFotoProvider(db.connection()));
    engine.loadFromModule("Distribuidora", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning("Main.qml nao carregou");
        return 1;
    }

    auto *janela = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!janela) {
        qWarning("a raiz nao e uma janela");
        return 1;
    }
    janela->resize(1280, 800);
    janela->show();
    assentar(700);

    qInfo("Capturando em %s", qPrintable(pasta));

    // 1) A tela de entrada, ainda sem ninguém logado.
    capturar(janela, pasta, QStringLiteral("01-login"));

    // 2) Agora logado, percorre as telas na ordem em que o manual as ensina.
    backend.login(QStringLiteral("davi"), QStringLiteral("davi12345"));
    semear(backend, db.connection());
    assentar(700);

    const QVector<QPair<QString, QString>> roteiro = {
        {QStringLiteral("dashboard"),  QStringLiteral("02-dashboard")},
        {QStringLiteral("produtos"),   QStringLiteral("03-produtos")},
        {QStringLiteral("estoque"),    QStringLiteral("04-estoque")},
        {QStringLiteral("compras"),    QStringLiteral("05-compras")},
        {QStringLiteral("caixa"),      QStringLiteral("06-caixa")},
        {QStringLiteral("pdv"),        QStringLiteral("07-pdv")},
        {QStringLiteral("vendas"),     QStringLiteral("08-vendas")},
        {QStringLiteral("clientes"),   QStringLiteral("09-clientes")},
        {QStringLiteral("financeiro"), QStringLiteral("10-financeiro")},
        {QStringLiteral("vencimento"), QStringLiteral("11-vencimento")},
        {QStringLiteral("relatorios"), QStringLiteral("12-relatorios")},
        {QStringLiteral("usuarios"),   QStringLiteral("13-usuarios")},
        {QStringLiteral("backup"),     QStringLiteral("14-backup")},
    };

    for (const auto &par : roteiro) {
        QMetaObject::invokeMethod(janela, "irPara", Q_ARG(QVariant, par.first));
        capturar(janela, pasta, par.second);

        if (par.first == QStringLiteral("relatorios")) {
            QQuickItem *tela = acharTela(janela, QStringLiteral("RelatoriosScreen"));
            QObject *periodo = tela ? tela->findChild<QObject *>(QStringLiteral("periodoRelatorio")) : nullptr;
            if (periodo) {
                periodo->setProperty("currentIndex", 3);
                capturar(janela, pasta, QStringLiteral("12b-relatorios-outro-dia"));
                if (QObject *cal = tela->findChild<QObject *>(QStringLiteral("calendarioRelatorio"))) {
                    QMetaObject::invokeMethod(cal, "abrirEm",
                        Q_ARG(QVariant, QDate::currentDate().addDays(-1).toString(Qt::ISODate)));
                    capturar(janela, pasta, QStringLiteral("12c-relatorios-calendario"));
                    QMetaObject::invokeMethod(cal, "close");
                }
                periodo->setProperty("currentIndex", 0);
            }
        }

        if (par.first == QStringLiteral("estoque")) {
            backend.estoque()->setFiltroStatus(QStringLiteral("zerado"));
            capturar(janela, pasta, QStringLiteral("04b-estoque-filtro-zerados"));
            backend.estoque()->setFiltroStatus(QString());
        }

        // O PDV vazio mostra onde bipar, mas quem esta aprendendo precisa ver
        // como fica DEPOIS de bipar. Enche o carrinho e captura de novo.
        if (par.first == QStringLiteral("pdv")) {
            QQuickItem *pdv = acharTela(janela, QStringLiteral("PdvScreen"));
            if (!pdv) {
                qWarning("nao achei a PdvScreen para montar o carrinho de exemplo");
            } else {
                const QStringList codigos = {QStringLiteral("7896045506873"),
                                             QStringLiteral("7891991010023"),
                                             QStringLiteral("7898934500115")};
                for (const QString &cod : codigos) {
                    const QVariantMap item = backend.buscarProdutoPorCodigo(cod);
                    if (!item.value(QStringLiteral("encontrado")).toBool()) {
                        qWarning("codigo %s nao encontrado", qPrintable(cod));
                        continue;
                    }
                    QMetaObject::invokeMethod(pdv, "adicionar", Q_ARG(QVariant, item));
                }
                assentar(200);
                capturar(janela, pasta, QStringLiteral("07b-pdv-carrinho"));
            }
        }
    }

    // Segundo argumento "auditoria": as imagens do relatório de correções.
    if (argc > 2 && QString::fromLocal8Bit(argv[2]) == QLatin1String("auditoria"))
        capturarAuditoria(janela, backend, db.connection(), pasta);

    qInfo("Pronto.");
    return 0;
}
