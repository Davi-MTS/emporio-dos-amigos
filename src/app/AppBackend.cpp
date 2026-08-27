#include "app/AppBackend.h"

#include "utils/Money.h"

#include <utility>

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

AppBackend::AppBackend(QSqlDatabase db, QObject *parent)
    : QObject(parent)
    , m_db(std::move(db))
    , m_produtoRepo(m_db)
    , m_estoqueRepo(m_db)
    , m_caixaRepo(m_db)
    , m_vendaRepo(m_db)
    , m_usuarioRepo(m_db)
    , m_fornecedorRepo(m_db)
    , m_compraRepo(m_db)
    , m_clienteRepo(m_db)
    , m_financeiroRepo(m_db)
    , m_relatorioRepo(m_db)
    , m_backupService(m_db)
    , m_relatorioMobile(m_db)
    , m_produtosModel(new ProdutosListModel(this))
    , m_estoqueModel(new EstoqueListModel(this))
    , m_usuariosModel(new UsuariosListModel(this))
    , m_fornecedoresModel(new FornecedoresListModel(this))
    , m_comprasModel(new ComprasListModel(this))
    , m_clientesModel(new ClientesListModel(this))
    , m_contasPagarModel(new ContasPagarModel(this))
    , m_contasReceberModel(new ContasReceberModel(this))
{
    m_sessaoId = m_caixaRepo.sessaoAbertaId();
    recarregarProdutos();
    recarregarEstoque();
}

bool AppBackend::precisaCriarAdmin()
{
    return m_usuarioRepo.contarComSenha() == 0;
}

void AppBackend::_definirUsuarioAtual(const Usuario &u)
{
    m_usuarioId = u.id;
    QVariantMap m;
    m[QStringLiteral("id")] = u.id;
    m[QStringLiteral("nome")] = u.nome;
    m[QStringLiteral("login")] = u.login;
    m[QStringLiteral("perfil")] = u.perfilNome;
    const QJsonDocument doc = QJsonDocument::fromJson(u.permissoesJson.toUtf8());
    m[QStringLiteral("permissoes")] = doc.object().toVariantMap();
    m_usuarioAtual = m;
    emit sessaoUsuarioChanged();
}

bool AppBackend::login(const QString &login, const QString &senha)
{
    const auto u = m_usuarioRepo.autenticar(login, senha);
    if (!u) {
        m_erro = QStringLiteral("Login ou senha inválidos.");
        return false;
    }
    _definirUsuarioAtual(*u);
    m_erro.clear();
    return true;
}

bool AppBackend::criarAdmin(const QString &nome, const QString &login, const QString &senha)
{
    if (!m_usuarioRepo.criarPrimeiroAdmin(nome, login, senha)) {
        m_erro = m_usuarioRepo.ultimoErro();
        return false;
    }
    // Autentica automaticamente o admin recém-criado.
    const auto u = m_usuarioRepo.autenticar(login, senha);
    if (u)
        _definirUsuarioAtual(*u);
    m_erro.clear();
    return true;
}

void AppBackend::logout()
{
    m_usuarioId = 0;
    m_usuarioAtual = QVariantMap();
    emit sessaoUsuarioChanged();
}

bool AppBackend::temPermissao(const QString &chave) const
{
    const QVariantMap perms = m_usuarioAtual.value(QStringLiteral("permissoes")).toMap();
    if (perms.value(QStringLiteral("tudo")).toBool())
        return true;
    return perms.value(chave).toBool();
}

void AppBackend::recarregarUsuarios()
{
    m_usuariosModel->setUsuarios(m_usuarioRepo.listar());
}

QVariantMap AppBackend::usuario(int id)
{
    QVariantMap m;
    const auto u = m_usuarioRepo.obter(id);
    if (!u)
        return m;
    m[QStringLiteral("id")] = u->id;
    m[QStringLiteral("nome")] = u->nome;
    m[QStringLiteral("login")] = u->login;
    m[QStringLiteral("perfilId")] = u->perfilId;
    return m;
}

QVariantMap AppBackend::novoUsuario()
{
    QVariantMap m;
    m[QStringLiteral("id")] = 0;
    m[QStringLiteral("nome")] = QString();
    m[QStringLiteral("login")] = QString();
    m[QStringLiteral("perfilId")] = 2; // Funcionário por padrão
    return m;
}

bool AppBackend::salvarUsuario(const QVariantMap &dados, const QString &senha)
{
    Usuario u;
    u.id = dados.value(QStringLiteral("id")).toInt();
    u.nome = dados.value(QStringLiteral("nome")).toString();
    u.login = dados.value(QStringLiteral("login")).toString();
    u.perfilId = dados.value(QStringLiteral("perfilId")).toInt();
    if (!m_usuarioRepo.salvar(u, senha)) {
        m_erro = m_usuarioRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarUsuarios();
    return true;
}

bool AppBackend::inativarUsuario(int id)
{
    if (id == m_usuarioId) {
        m_erro = QStringLiteral("Você não pode desativar o próprio usuário logado.");
        return false;
    }
    if (!m_usuarioRepo.inativar(id)) {
        m_erro = m_usuarioRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarUsuarios();
    return true;
}

QVariantList AppBackend::perfis()
{
    QVariantList lista;
    const auto ps = m_usuarioRepo.listarPerfis();
    for (const auto &p : ps) {
        QVariantMap m;
        m[QStringLiteral("id")] = p.first;
        m[QStringLiteral("nome")] = p.second;
        lista.push_back(m);
    }
    return lista;
}

// ---------------------------------------------------------- Compras / fornecedores

void AppBackend::recarregarFornecedores(const QString &filtro)
{
    m_fornecedoresModel->setFornecedores(m_fornecedorRepo.listar(filtro));
}

QVariantList AppBackend::fornecedoresLista()
{
    QVariantList lista;
    const auto fs = m_fornecedorRepo.listar();
    for (const Fornecedor &f : fs) {
        QVariantMap m;
        m[QStringLiteral("id")] = f.id;
        m[QStringLiteral("nome")] = f.nome;
        lista.push_back(m);
    }
    return lista;
}

QVariantMap AppBackend::fornecedor(int id)
{
    QVariantMap m;
    const auto f = m_fornecedorRepo.obter(id);
    if (!f)
        return m;
    m[QStringLiteral("id")] = f->id;
    m[QStringLiteral("nome")] = f->nome;
    m[QStringLiteral("cnpj")] = f->cnpj;
    m[QStringLiteral("contato")] = f->contato;
    m[QStringLiteral("telefone")] = f->telefone;
    m[QStringLiteral("email")] = f->email;
    m[QStringLiteral("endereco")] = f->endereco;
    return m;
}

QVariantMap AppBackend::novoFornecedor()
{
    QVariantMap m;
    m[QStringLiteral("id")] = 0;
    m[QStringLiteral("nome")] = QString();
    m[QStringLiteral("cnpj")] = QString();
    m[QStringLiteral("contato")] = QString();
    m[QStringLiteral("telefone")] = QString();
    m[QStringLiteral("email")] = QString();
    m[QStringLiteral("endereco")] = QString();
    return m;
}

bool AppBackend::salvarFornecedor(const QVariantMap &dados)
{
    Fornecedor f;
    f.id = dados.value(QStringLiteral("id")).toInt();
    f.nome = dados.value(QStringLiteral("nome")).toString();
    f.cnpj = dados.value(QStringLiteral("cnpj")).toString();
    f.contato = dados.value(QStringLiteral("contato")).toString();
    f.telefone = dados.value(QStringLiteral("telefone")).toString();
    f.email = dados.value(QStringLiteral("email")).toString();
    f.endereco = dados.value(QStringLiteral("endereco")).toString();
    if (!m_fornecedorRepo.salvar(f)) {
        m_erro = m_fornecedorRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarFornecedores();
    return true;
}

void AppBackend::recarregarCompras()
{
    m_comprasModel->setCompras(m_compraRepo.listar());
}

QVariantMap AppBackend::registrarCompra(const QVariantMap &dados)
{
    QVector<ItemCompra> itens;
    const QVariantList itensIn = dados.value(QStringLiteral("itens")).toList();
    for (const QVariant &v : itensIn) {
        const QVariantMap im = v.toMap();
        ItemCompra it;
        it.produtoId = im.value(QStringLiteral("produtoId")).toInt();
        it.embalagemId = im.value(QStringLiteral("embalagemId")).toInt();
        it.fator = im.value(QStringLiteral("fator"), 1).toInt();
        it.qtdEmbalagem = im.value(QStringLiteral("qtd")).toLongLong();
        it.custoUnitEmbalagem = im.value(QStringLiteral("custo")).toLongLong();
        itens.push_back(it);
    }

    const int fornecedorId = dados.value(QStringLiteral("fornecedorId")).toInt();
    const bool gerarConta = dados.value(QStringLiteral("gerarContaPagar")).toBool();
    const QString vencimento = dados.value(QStringLiteral("vencimento")).toString();
    const QString numeroNota = dados.value(QStringLiteral("numeroNota")).toString();
    const QString dataNota = dados.value(QStringLiteral("dataNota")).toString();

    const ResultadoCompra r = m_compraRepo.registrarCompra(
        fornecedorId, QStringLiteral("manual"), itens, gerarConta, vencimento, m_usuarioId,
        numeroNota, dataNota);

    QVariantMap out;
    out[QStringLiteral("ok")] = r.ok;
    out[QStringLiteral("compraId")] = r.compraId;
    out[QStringLiteral("total")] = static_cast<qlonglong>(r.total);
    out[QStringLiteral("erro")] = r.erro;
    if (r.ok) {
        recarregarCompras();
        recarregarEstoque();
        recarregarProdutos();
    } else {
        m_erro = r.erro;
    }
    return out;
}

// ---------------------------------------------------------------- Clientes / fiado

void AppBackend::recarregarClientes(const QString &filtro)
{
    m_clientesModel->setClientes(m_clienteRepo.listar(filtro));
}

QVariantList AppBackend::clientesLista()
{
    QVariantList lista;
    const auto cs = m_clienteRepo.listar();
    for (const Cliente &c : cs) {
        QVariantMap m;
        m[QStringLiteral("id")] = c.id;
        m[QStringLiteral("nome")] = c.nome;
        m[QStringLiteral("saldo")] = static_cast<qlonglong>(c.saldoDevedor);
        m[QStringLiteral("limite")] = static_cast<qlonglong>(c.limiteFiado);
        lista.push_back(m);
    }
    return lista;
}

QVariantMap AppBackend::cliente(int id)
{
    QVariantMap m;
    const auto c = m_clienteRepo.obter(id);
    if (!c)
        return m;
    m[QStringLiteral("id")] = c->id;
    m[QStringLiteral("nome")] = c->nome;
    m[QStringLiteral("telefone")] = c->telefone;
    m[QStringLiteral("cpf")] = c->cpf;
    m[QStringLiteral("endereco")] = c->endereco;
    m[QStringLiteral("aniversario")] = c->aniversario;
    m[QStringLiteral("observacoes")] = c->observacoes;
    m[QStringLiteral("limite")] = static_cast<qlonglong>(c->limiteFiado);
    m[QStringLiteral("saldo")] = static_cast<qlonglong>(c->saldoDevedor);
    return m;
}

QVariantMap AppBackend::novoCliente()
{
    QVariantMap m;
    m[QStringLiteral("id")] = 0;
    m[QStringLiteral("nome")] = QString();
    m[QStringLiteral("telefone")] = QString();
    m[QStringLiteral("cpf")] = QString();
    m[QStringLiteral("endereco")] = QString();
    m[QStringLiteral("aniversario")] = QString();
    m[QStringLiteral("observacoes")] = QString();
    m[QStringLiteral("limite")] = 0;
    m[QStringLiteral("saldo")] = 0;
    return m;
}

bool AppBackend::salvarCliente(const QVariantMap &dados)
{
    Cliente c;
    c.id = dados.value(QStringLiteral("id")).toInt();
    c.nome = dados.value(QStringLiteral("nome")).toString();
    c.telefone = dados.value(QStringLiteral("telefone")).toString();
    c.cpf = dados.value(QStringLiteral("cpf")).toString();
    c.endereco = dados.value(QStringLiteral("endereco")).toString();
    c.aniversario = dados.value(QStringLiteral("aniversario")).toString();
    c.observacoes = dados.value(QStringLiteral("observacoes")).toString();
    c.limiteFiado = dados.value(QStringLiteral("limite")).toLongLong();
    if (!m_clienteRepo.salvar(c)) {
        m_erro = m_clienteRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarClientes();
    return true;
}

bool AppBackend::inativarCliente(int id)
{
    if (!m_clienteRepo.inativar(id)) {
        m_erro = m_clienteRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarClientes();
    return true;
}

QVariantMap AppBackend::receberDeCliente(int clienteId, const QString &valorTexto,
                                         const QString &forma)
{
    QVariantMap out;
    out[QStringLiteral("ok")] = false;
    out[QStringLiteral("aplicado")] = 0;

    const qint64 saldo = m_clienteRepo.saldoDevedor(clienteId);
    if (saldo <= 0) {
        out[QStringLiteral("erro")] = QStringLiteral("Cliente sem dívida em aberto.");
        return out;
    }

    qint64 valor = saldo;
    if (!valorTexto.trimmed().isEmpty()) {
        const auto v = Money::parse(valorTexto);
        if (!v || *v <= 0) {
            out[QStringLiteral("erro")] = QStringLiteral("Valor inválido.");
            return out;
        }
        valor = *v;
    }
    if (valor > saldo) // nunca recebe mais que a dívida
        valor = saldo;

    if (!m_db.transaction()) {
        out[QStringLiteral("erro")] = m_db.lastError().text();
        return out;
    }
    const qint64 aplicado = m_clienteRepo.aplicarRecebimento(clienteId, valor);
    if (aplicado <= 0) {
        m_db.rollback();
        const QString e = m_clienteRepo.ultimoErro();
        out[QStringLiteral("erro")] = e.isEmpty() ? QStringLiteral("Nada a receber.") : e;
        return out;
    }
    // Dinheiro recebido entra na gaveta (só se em dinheiro e com caixa aberto).
    if (forma == QStringLiteral("dinheiro") && m_sessaoId > 0) {
        if (!m_caixaRepo.registrarRecebimento(m_sessaoId, aplicado,
                                              QStringLiteral("Recebimento de fiado"), m_usuarioId)) {
            m_db.rollback();
            out[QStringLiteral("erro")] = m_caixaRepo.ultimoErro();
            return out;
        }
    }
    if (!m_db.commit()) {
        m_db.rollback();
        out[QStringLiteral("erro")] = m_db.lastError().text();
        return out;
    }

    m_erro.clear();
    recarregarClientes();
    recarregarFinanceiro();
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("aplicado")] = static_cast<qlonglong>(aplicado);
    out[QStringLiteral("erro")] = QString();
    return out;
}

// ---------------------------------------------------------------- Financeiro

void AppBackend::recarregarFinanceiro()
{
    m_contasPagarModel->setContas(m_financeiroRepo.contasPagar(/*apenasAbertas=*/true));
    m_contasReceberModel->setContas(m_financeiroRepo.contasReceber(/*apenasAbertas=*/true));
}

QVariantMap AppBackend::resumoFinanceiro()
{
    const ResumoFinanceiro r = m_financeiroRepo.resumo();
    QVariantMap m;
    m[QStringLiteral("aPagar")] = static_cast<qlonglong>(r.totalAPagar);
    m[QStringLiteral("aReceber")] = static_cast<qlonglong>(r.totalAReceber);
    m[QStringLiteral("saldo")] = static_cast<qlonglong>(r.saldoPrevisto());
    return m;
}

bool AppBackend::pagarConta(int id, const QString &forma)
{
    // Lê valor/descrição antes de pagar (para lançar a sangria, se em dinheiro).
    qint64 valor = 0;
    QString descricao;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT valor, COALESCE(descricao, 'Compra #' || compra_id) "
            "FROM contas_pagar WHERE id = :id AND status = 'aberta'"));
        q.bindValue(QStringLiteral(":id"), id);
        if (q.exec() && q.next()) {
            valor = q.value(0).toLongLong();
            descricao = q.value(1).toString();
        }
    }

    if (!m_db.transaction()) {
        m_erro = m_db.lastError().text();
        return false;
    }
    if (!m_financeiroRepo.pagar(id)) {
        m_db.rollback();
        m_erro = m_financeiroRepo.ultimoErro();
        return false;
    }
    // Pagamento em dinheiro sai da gaveta -> registra sangria no caixa aberto.
    if (forma == QStringLiteral("dinheiro") && m_sessaoId > 0 && valor > 0) {
        if (!m_caixaRepo.registrarMovimento(m_sessaoId, QStringLiteral("sangria"), valor,
                                            QStringLiteral("Pagamento: %1").arg(descricao),
                                            m_usuarioId)) {
            m_db.rollback();
            m_erro = m_caixaRepo.ultimoErro();
            return false;
        }
    }
    if (!m_db.commit()) {
        m_db.rollback();
        m_erro = m_db.lastError().text();
        return false;
    }
    m_erro.clear();
    recarregarFinanceiro();
    return true;
}

QVariantMap AppBackend::receberContaValor(int id, const QString &valorTexto,
                                          const QString &forma)
{
    QVariantMap out;
    out[QStringLiteral("ok")] = false;
    out[QStringLiteral("aplicado")] = 0;

    // Vazio = recebe o valor cheio da conta (o repositório limita ao saldo dela).
    qint64 valor = Q_INT64_C(1000000000000000);
    if (!valorTexto.trimmed().isEmpty()) {
        const auto v = Money::parse(valorTexto);
        if (!v || *v <= 0) {
            out[QStringLiteral("erro")] = QStringLiteral("Valor inválido.");
            return out;
        }
        valor = *v;
    }

    if (!m_db.transaction()) {
        out[QStringLiteral("erro")] = m_db.lastError().text();
        return out;
    }
    const qint64 aplicado = m_financeiroRepo.aplicarRecebimentoConta(id, valor);
    if (aplicado <= 0) {
        m_db.rollback();
        const QString e = m_financeiroRepo.ultimoErro();
        out[QStringLiteral("erro")] = e.isEmpty() ? QStringLiteral("Conta já recebida.") : e;
        return out;
    }
    if (forma == QStringLiteral("dinheiro") && m_sessaoId > 0) {
        if (!m_caixaRepo.registrarRecebimento(m_sessaoId, aplicado,
                                              QStringLiteral("Recebimento de fiado"), m_usuarioId)) {
            m_db.rollback();
            out[QStringLiteral("erro")] = m_caixaRepo.ultimoErro();
            return out;
        }
    }
    if (!m_db.commit()) {
        m_db.rollback();
        out[QStringLiteral("erro")] = m_db.lastError().text();
        return out;
    }

    m_erro.clear();
    recarregarFinanceiro();
    recarregarClientes(); // o saldo do cliente pode mudar
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("aplicado")] = static_cast<qlonglong>(aplicado);
    out[QStringLiteral("erro")] = QString();
    return out;
}

bool AppBackend::criarDespesa(const QString &descricao, const QString &valorTexto,
                              const QString &vencimento)
{
    const auto v = Money::parse(valorTexto);
    if (!v) {
        m_erro = QStringLiteral("Valor inválido.");
        return false;
    }
    if (!m_financeiroRepo.criarDespesa(descricao, *v, vencimento)) {
        m_erro = m_financeiroRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarFinanceiro();
    return true;
}

// ---------------------------------------------------------------- Relatórios

QVariantMap AppBackend::dashboard()
{
    const DashboardKpis k = m_relatorioRepo.dashboard();
    QVariantMap m;
    m[QStringLiteral("vendasHoje")] = static_cast<qlonglong>(k.vendasHoje);
    m[QStringLiteral("numVendasHoje")] = k.numVendasHoje;
    m[QStringLiteral("ticketMedio")] = static_cast<qlonglong>(k.ticketMedio);
    m[QStringLiteral("produtosEmFalta")] = k.produtosEmFalta;
    m[QStringLiteral("aReceber")] = static_cast<qlonglong>(k.aReceber);
    return m;
}

QVariantMap AppBackend::relatorioFaturamento(int dias)
{
    const FaturamentoResumo r = m_relatorioRepo.faturamento(dias);
    QVariantMap m;
    m[QStringLiteral("total")] = static_cast<qlonglong>(r.total);
    m[QStringLiteral("numVendas")] = r.numVendas;
    m[QStringLiteral("ticket")] = static_cast<qlonglong>(r.ticket);
    m[QStringLiteral("custo")] = static_cast<qlonglong>(r.custo);
    m[QStringLiteral("lucro")] = static_cast<qlonglong>(r.lucro);
    return m;
}

QVariantList AppBackend::relatorioFormas(int dias)
{
    QVariantList lista;
    const auto fs = m_relatorioRepo.vendasPorForma(dias);
    for (const FormaTotal &f : fs) {
        QVariantMap m;
        m[QStringLiteral("forma")] = f.forma;
        m[QStringLiteral("total")] = static_cast<qlonglong>(f.total);
        lista.push_back(m);
    }
    return lista;
}

QVariantList AppBackend::relatorioMaisVendidos(int dias, int limite)
{
    QVariantList lista;
    const auto ps = m_relatorioRepo.maisVendidos(dias, limite);
    for (const ProdutoVendido &p : ps) {
        QVariantMap m;
        m[QStringLiteral("nome")] = p.nome;
        m[QStringLiteral("qtd")] = static_cast<qlonglong>(p.qtd);
        lista.push_back(m);
    }
    return lista;
}

QVariantList AppBackend::relatorioProdutosParados(int dias)
{
    QVariantList lista;
    const auto ps = m_relatorioRepo.produtosParados(dias);
    for (const ProdutoParado &p : ps) {
        QVariantMap m;
        m[QStringLiteral("nome")] = p.nome;
        m[QStringLiteral("estoque")] = static_cast<qlonglong>(p.estoque);
        lista.push_back(m);
    }
    return lista;
}

void AppBackend::recarregarProdutos(const QString &filtro)
{
    m_produtosModel->setProdutos(m_produtoRepo.listar(filtro));
}

QVariantList AppBackend::categorias()
{
    QVariantList lista;
    const auto cats = m_produtoRepo.listarCategorias();
    for (const auto &c : cats) {
        QVariantMap m;
        m[QStringLiteral("id")] = c.first;
        m[QStringLiteral("nome")] = c.second;
        lista.push_back(m);
    }
    return lista;
}

QVariantList AppBackend::composicaoParaVenda(int produtoId)
{
    QVariantList lista;
    const auto p = m_produtoRepo.obter(produtoId);
    if (!p)
        return lista;
    for (const Componente &c : p->composicao) {
        QVariantMap linha;
        linha[QStringLiteral("categoriaId")] = c.categoriaId;
        linha[QStringLiteral("categoriaNome")] = c.categoriaNome;
        linha[QStringLiteral("unidade")] = c.unidade;
        linha[QStringLiteral("quantidade")] = c.quantidade;

        QVariantList produtos;
        const auto ps = m_produtoRepo.produtosDaCategoria(c.categoriaId);
        for (const auto &pr : ps) {
            QVariantMap m;
            m[QStringLiteral("id")] = pr.first;
            m[QStringLiteral("nome")] = pr.second;
            produtos.push_back(m);
        }
        linha[QStringLiteral("produtos")] = produtos;
        lista.push_back(linha);
    }
    return lista;
}

static QVariantMap embalagemParaMapa(const Embalagem &e)
{
    QVariantMap m;
    m[QStringLiteral("id")] = e.id;
    m[QStringLiteral("nome")] = e.nome;
    m[QStringLiteral("fator")] = e.fator;
    m[QStringLiteral("codigoBarras")] = e.codigoBarras;
    m[QStringLiteral("preco")] = static_cast<qlonglong>(e.precoVenda);          // centavos
    m[QStringLiteral("custo")] = static_cast<qlonglong>(e.custoCompra);         // -1 = sem custo
    return m;
}

QVariantMap AppBackend::produto(int id)
{
    QVariantMap mapa;
    const auto p = m_produtoRepo.obter(id);
    if (!p)
        return mapa;

    mapa[QStringLiteral("id")] = p->id;
    mapa[QStringLiteral("nome")] = p->nome;
    mapa[QStringLiteral("categoriaId")] = p->categoriaId;
    mapa[QStringLiteral("unidadeBase")] = p->unidadeBase;
    mapa[QStringLiteral("estoqueMinimo")] = p->estoqueMinimo;
    mapa[QStringLiteral("localizacao")] = p->localizacao;
    mapa[QStringLiteral("composto")] = p->composto;

    QVariantList emb;
    for (const Embalagem &e : p->embalagens)
        emb.push_back(embalagemParaMapa(e));
    mapa[QStringLiteral("embalagens")] = emb;

    QVariantList comp;
    for (const Componente &c : p->composicao) {
        QVariantMap m;
        m[QStringLiteral("categoriaId")] = c.categoriaId;
        m[QStringLiteral("categoriaNome")] = c.categoriaNome;
        m[QStringLiteral("unidade")] = c.unidade;
        m[QStringLiteral("quantidade")] = c.quantidade;
        comp.push_back(m);
    }
    mapa[QStringLiteral("composicao")] = comp;
    return mapa;
}

QVariantMap AppBackend::novoProduto()
{
    QVariantMap mapa;
    mapa[QStringLiteral("id")] = 0;
    mapa[QStringLiteral("nome")] = QString();
    mapa[QStringLiteral("categoriaId")] = 0;
    mapa[QStringLiteral("unidadeBase")] = QStringLiteral("unidade");
    mapa[QStringLiteral("estoqueMinimo")] = 0;
    mapa[QStringLiteral("localizacao")] = QString();
    mapa[QStringLiteral("composto")] = false;
    mapa[QStringLiteral("composicao")] = QVariantList{};

    // Já nasce com a embalagem base (fator 1) — praticidade: o comerciante só
    // preenche nome, código e preço para vender por unidade.
    QVariantMap base;
    base[QStringLiteral("id")] = 0;
    base[QStringLiteral("nome")] = QStringLiteral("Unidade");
    base[QStringLiteral("fator")] = 1;
    base[QStringLiteral("codigoBarras")] = QString();
    base[QStringLiteral("preco")] = 0;
    base[QStringLiteral("custo")] = -1;
    mapa[QStringLiteral("embalagens")] = QVariantList{base};
    return mapa;
}

bool AppBackend::salvarProduto(const QVariantMap &dados)
{
    Produto p;
    p.id = dados.value(QStringLiteral("id")).toInt();
    p.nome = dados.value(QStringLiteral("nome")).toString();
    p.categoriaId = dados.value(QStringLiteral("categoriaId")).toInt();
    p.unidadeBase = dados.value(QStringLiteral("unidadeBase"), QStringLiteral("unidade")).toString();
    p.estoqueMinimo = dados.value(QStringLiteral("estoqueMinimo")).toInt();
    p.localizacao = dados.value(QStringLiteral("localizacao")).toString();
    p.composto = dados.value(QStringLiteral("composto")).toBool();

    const QVariantList comp = dados.value(QStringLiteral("composicao")).toList();
    for (const QVariant &item : comp) {
        const QVariantMap cm = item.toMap();
        Componente c;
        c.categoriaId = cm.value(QStringLiteral("categoriaId")).toInt();
        c.unidade = cm.value(QStringLiteral("unidade"), QStringLiteral("unidade")).toString();
        c.quantidade = cm.value(QStringLiteral("quantidade"), 1).toInt();
        if (c.categoriaId > 0 && c.quantidade > 0)
            p.composicao.push_back(c);
    }

    const QVariantList emb = dados.value(QStringLiteral("embalagens")).toList();
    for (const QVariant &item : emb) {
        const QVariantMap em = item.toMap();
        Embalagem e;
        e.id = em.value(QStringLiteral("id")).toInt();
        e.nome = em.value(QStringLiteral("nome")).toString();
        e.fator = em.value(QStringLiteral("fator"), 1).toInt();
        e.codigoBarras = em.value(QStringLiteral("codigoBarras")).toString();
        e.precoVenda = em.value(QStringLiteral("preco")).toLongLong();
        e.custoCompra = em.contains(QStringLiteral("custo"))
                            ? em.value(QStringLiteral("custo")).toLongLong()
                            : -1;
        if (e.nome.trimmed().isEmpty())
            continue; // ignora linhas de embalagem em branco
        p.embalagens.push_back(e);
    }

    if (!m_produtoRepo.salvar(p)) {
        m_erro = m_produtoRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarProdutos();
    recarregarEstoque();   // produto novo cria linha de estoque
    return true;
}

bool AppBackend::inativarProduto(int id)
{
    if (!m_produtoRepo.inativar(id)) {
        m_erro = m_produtoRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarProdutos();
    recarregarEstoque();
    return true;
}

// ---------------------------------------------------------------- Estoque

void AppBackend::recarregarEstoque(const QString &filtro)
{
    m_estoqueModel->setItens(m_estoqueRepo.listar(filtro));
}

QVariantMap AppBackend::itemEstoque(int produtoId)
{
    const ItemEstoque it = m_estoqueRepo.item(produtoId);
    QVariantMap m;
    m[QStringLiteral("produtoId")] = it.produtoId;
    m[QStringLiteral("nome")] = it.nome;
    m[QStringLiteral("localizacao")] = it.localizacao;
    m[QStringLiteral("unidadeBase")] = it.unidadeBase;
    m[QStringLiteral("quantidade")] = static_cast<qlonglong>(it.quantidade);
    m[QStringLiteral("minimo")] = it.minimo;
    m[QStringLiteral("custoMedio")] = static_cast<qlonglong>(it.custoMedio);
    return m;
}

qlonglong AppBackend::estoqueDisponivel(int produtoId)
{
    return static_cast<qlonglong>(m_estoqueRepo.item(produtoId).quantidade);
}

QVariantList AppBackend::embalagensDe(int produtoId)
{
    QVariantList lista;
    const auto p = m_produtoRepo.obter(produtoId);
    if (!p)
        return lista;
    for (const Embalagem &e : p->embalagens) {
        QVariantMap m;
        m[QStringLiteral("id")] = e.id;
        m[QStringLiteral("nome")] = e.nome;
        m[QStringLiteral("fator")] = e.fator;
        // custo_compra cadastrado na embalagem (-1 se não informado): serve para
        // pré-preencher o custo na tela de Compras e agilizar entrada precisa.
        m[QStringLiteral("custo")] = static_cast<qlonglong>(e.custoCompra);
        m[QStringLiteral("preco")] = static_cast<qlonglong>(e.precoVenda);
        lista.push_back(m);
    }
    return lista;
}

bool AppBackend::registrarEntrada(int produtoId, int embalagemId, int qtdEmb,
                                  const QString &custoTexto, const QString &observacao)
{
    int fator = 1;
    const auto p = m_produtoRepo.obter(produtoId);
    if (p) {
        for (const Embalagem &e : p->embalagens) {
            if (e.id == embalagemId) {
                fator = e.fator > 0 ? e.fator : 1;
                break;
            }
        }
    }
    const qint64 qtdBase = static_cast<qint64>(qtdEmb) * fator;

    // Custo informado é por embalagem; converte para custo por unidade base em
    // MILÉSIMOS de centavo (×1000 antes de dividir pelo fator, p/ não perder ml).
    qint64 custoUnitBaseMilli = -1;
    const QString ct = custoTexto.trimmed();
    if (!ct.isEmpty()) {
        const auto cents = Money::parse(ct);
        if (cents && fator > 0)
            custoUnitBaseMilli = *cents * 1000 / fator;
    }

    if (!m_estoqueRepo.registrarEntradaMilli(produtoId, qtdBase, custoUnitBaseMilli, 0, observacao)) {
        m_erro = m_estoqueRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarEstoque();
    recarregarProdutos();
    return true;
}

bool AppBackend::registrarInventario(int produtoId, int novaQtdBase, const QString &motivo)
{
    if (!m_estoqueRepo.registrarInventario(produtoId, novaQtdBase, motivo, 0)) {
        m_erro = m_estoqueRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarEstoque();
    recarregarProdutos();
    return true;
}

bool AppBackend::registrarRetirada(int produtoId, int embalagemId, int qtdEmb,
                                   const QString &motivo)
{
    int fator = 1;
    const auto p = m_produtoRepo.obter(produtoId);
    if (p) {
        for (const Embalagem &e : p->embalagens) {
            if (e.id == embalagemId) {
                fator = e.fator > 0 ? e.fator : 1;
                break;
            }
        }
    }
    const qint64 qtdBase = static_cast<qint64>(qtdEmb) * fator;

    if (!m_estoqueRepo.registrarSaida(produtoId, qtdBase, motivo, 0)) {
        m_erro = m_estoqueRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarEstoque();
    recarregarProdutos();
    return true;
}

// ---------------------------------------------------------------- PDV / Vendas

bool AppBackend::abrirCaixa(const QString &valorAberturaTexto)
{
    qint64 valor = 0;
    const auto v = Money::parse(valorAberturaTexto);
    if (v)
        valor = *v;
    const int id = m_caixaRepo.abrirSessao(valor, m_usuarioId);
    if (id <= 0) {
        m_erro = m_caixaRepo.ultimoErro();
        return false;
    }
    m_sessaoId = id;
    m_erro.clear();
    emit caixaAbertoChanged();
    return true;
}

static QVariantMap itemVendaMapa(const Produto &p, const Embalagem &e)
{
    QVariantMap m;
    m[QStringLiteral("encontrado")] = true;
    m[QStringLiteral("produtoId")] = p.id;
    m[QStringLiteral("nome")] = p.nome;
    m[QStringLiteral("composto")] = p.composto;
    m[QStringLiteral("unidadeBase")] = p.unidadeBase;
    m[QStringLiteral("embalagemId")] = e.id;
    m[QStringLiteral("embalagemNome")] = e.nome;
    m[QStringLiteral("fator")] = e.fator;
    m[QStringLiteral("preco")] = static_cast<qlonglong>(e.precoVenda);
    return m;
}

QVariantMap AppBackend::buscarProdutoPorCodigo(const QString &codigo)
{
    const auto achado = m_produtoRepo.buscarPorCodigoBarras(codigo);
    if (!achado) {
        QVariantMap m;
        m[QStringLiteral("encontrado")] = false;
        return m;
    }
    return itemVendaMapa(achado->first, achado->second);
}

QVariantList AppBackend::buscarProdutosPorNome(const QString &termo, bool incluirCompostos)
{
    QVariantList lista;
    if (termo.trimmed().size() < 2)
        return lista;

    const auto produtos = m_produtoRepo.listar(termo);
    int n = 0;
    for (const Produto &pl : produtos) {
        // Na compra, produto composto (copão/dose) não se compra — só os insumos.
        if (!incluirCompostos && pl.composto)
            continue;
        if (n++ >= 8)
            break;
        const auto full = m_produtoRepo.obter(pl.id);
        if (!full || full->embalagens.isEmpty())
            continue;
        // embalagens vêm ordenadas por fator asc: a primeira é a base.
        lista.push_back(itemVendaMapa(*full, full->embalagens.first()));
    }
    return lista;
}

QVariantMap AppBackend::finalizarVenda(const QVariantMap &dados)
{
    QVector<LinhaVenda> itens;
    const QVariantList itensIn = dados.value(QStringLiteral("itens")).toList();
    for (const QVariant &v : itensIn) {
        const QVariantMap im = v.toMap();
        LinhaVenda l;
        l.produtoId = im.value(QStringLiteral("produtoId")).toInt();
        l.embalagemId = im.value(QStringLiteral("embalagemId")).toInt();
        l.fator = im.value(QStringLiteral("fator"), 1).toInt();
        l.qtdEmbalagem = im.value(QStringLiteral("qtd")).toLongLong();
        l.precoUnit = im.value(QStringLiteral("precoUnit")).toLongLong();
        l.desconto = im.value(QStringLiteral("desconto")).toLongLong();

        const QVariantList insIn = im.value(QStringLiteral("insumos")).toList();
        for (const QVariant &iv : insIn) {
            const QVariantMap ins = iv.toMap();
            InsumoResolvido ir;
            ir.produtoId = ins.value(QStringLiteral("produtoId")).toInt();
            ir.quantidade = ins.value(QStringLiteral("quantidade")).toLongLong();
            if (ir.produtoId > 0 && ir.quantidade > 0)
                l.insumos.push_back(ir);
        }
        itens.push_back(l);
    }

    QVector<PagamentoVenda> pagamentos;
    const QVariantList pagsIn = dados.value(QStringLiteral("pagamentos")).toList();
    for (const QVariant &v : pagsIn) {
        const QVariantMap pm = v.toMap();
        PagamentoVenda p;
        p.forma = pm.value(QStringLiteral("forma")).toString();
        p.valor = pm.value(QStringLiteral("valor")).toLongLong();
        pagamentos.push_back(p);
    }

    const qint64 desconto = dados.value(QStringLiteral("desconto")).toLongLong();
    const int clienteId = dados.value(QStringLiteral("clienteId")).toInt();

    const ResultadoVenda r = m_vendaRepo.registrarVenda(
        m_sessaoId, clienteId, desconto, itens, pagamentos, m_usuarioId);

    QVariantMap out;
    out[QStringLiteral("ok")] = r.ok;
    out[QStringLiteral("vendaId")] = r.vendaId;
    out[QStringLiteral("total")] = static_cast<qlonglong>(r.total);
    out[QStringLiteral("troco")] = static_cast<qlonglong>(r.troco);
    out[QStringLiteral("erro")] = r.erro;

    if (r.ok) {
        recarregarEstoque();
        recarregarProdutos();
    } else {
        m_erro = r.erro;
    }
    return out;
}

QVariantMap AppBackend::caixaResumo()
{
    QVariantMap m;
    if (m_sessaoId <= 0) {
        m[QStringLiteral("aberto")] = false;
        return m;
    }
    const ResumoCaixa r = m_caixaRepo.resumo(m_sessaoId);
    m[QStringLiteral("aberto")] = true;
    m[QStringLiteral("abertura")] = static_cast<qlonglong>(r.abertura);
    m[QStringLiteral("vendasDinheiro")] = static_cast<qlonglong>(r.vendasDinheiro);
    m[QStringLiteral("vendasPix")] = static_cast<qlonglong>(r.vendasPix);
    m[QStringLiteral("vendasDebito")] = static_cast<qlonglong>(r.vendasDebito);
    m[QStringLiteral("vendasCredito")] = static_cast<qlonglong>(r.vendasCredito);
    m[QStringLiteral("vendasFiado")] = static_cast<qlonglong>(r.vendasFiado);
    m[QStringLiteral("suprimentos")] = static_cast<qlonglong>(r.suprimentos);
    m[QStringLiteral("sangrias")] = static_cast<qlonglong>(r.sangrias);
    m[QStringLiteral("recebimentos")] = static_cast<qlonglong>(r.recebimentos);
    m[QStringLiteral("troco")] = static_cast<qlonglong>(r.troco);
    m[QStringLiteral("numVendas")] = r.numVendas;
    m[QStringLiteral("totalVendas")] = static_cast<qlonglong>(r.totalVendas());
    m[QStringLiteral("dinheiroEsperado")] = static_cast<qlonglong>(r.dinheiroEsperado());
    return m;
}

bool AppBackend::registrarSangria(const QString &valorTexto, const QString &motivo)
{
    const auto v = Money::parse(valorTexto);
    if (!v) {
        m_erro = QStringLiteral("Valor inválido.");
        return false;
    }
    if (!m_caixaRepo.registrarMovimento(m_sessaoId, QStringLiteral("sangria"), *v, motivo, m_usuarioId)) {
        m_erro = m_caixaRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    return true;
}

bool AppBackend::registrarSuprimento(const QString &valorTexto, const QString &motivo)
{
    const auto v = Money::parse(valorTexto);
    if (!v) {
        m_erro = QStringLiteral("Valor inválido.");
        return false;
    }
    if (!m_caixaRepo.registrarMovimento(m_sessaoId, QStringLiteral("suprimento"), *v, motivo, m_usuarioId)) {
        m_erro = m_caixaRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    return true;
}

QVariantMap AppBackend::fecharCaixa(const QString &dinheiroContadoTexto)
{
    const auto v = Money::parse(dinheiroContadoTexto);
    const qint64 contado = v ? *v : 0;
    const ResultadoFechamento r = m_caixaRepo.fechar(m_sessaoId, contado, m_usuarioId);

    QVariantMap out;
    out[QStringLiteral("ok")] = r.ok;
    out[QStringLiteral("esperado")] = static_cast<qlonglong>(r.esperado);
    out[QStringLiteral("informado")] = static_cast<qlonglong>(r.informado);
    out[QStringLiteral("diferenca")] = static_cast<qlonglong>(r.diferenca);
    out[QStringLiteral("erro")] = r.erro;
    if (r.ok) {
        m_sessaoId = 0;
        emit caixaAbertoChanged();
        // Backup automático de fim de expediente (melhor esforço — nunca faz o
        // fechamento do caixa falhar). Mantém as 5 cópias mais recentes.
        if (m_backupService.criarBackup(nullptr))
            m_backupService.rotacionar(5);
        // Atualiza o relatório do celular (OneDrive) com os números do dia.
        m_relatorioMobile.gerar(nullptr);
    } else {
        m_erro = r.erro;
    }
    return out;
}

// ---------------------------------------------------------------- Backup

QVariantMap AppBackend::fazerBackup()
{
    QVariantMap out;
    BackupInfo info;
    if (!m_backupService.criarBackup(&info)) {
        out[QStringLiteral("ok")] = false;
        out[QStringLiteral("erro")] = m_backupService.ultimoErro();
        return out;
    }
    m_backupService.rotacionar(5);
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("caminho")] = info.caminho;
    out[QStringLiteral("resumo")] = info.resumo;
    out[QStringLiteral("erro")] = QString();
    return out;
}

QVariantList AppBackend::backupsDisponiveis()
{
    QVariantList lista;
    const auto backups = m_backupService.listarBackups();
    for (const BackupInfo &b : backups) {
        QVariantMap m;
        m[QStringLiteral("caminho")] = b.caminho;
        m[QStringLiteral("criadoEm")] = b.criadoEm;
        m[QStringLiteral("tamanho")] = static_cast<qlonglong>(b.tamanho);
        m[QStringLiteral("resumo")] = b.resumo;
        lista.push_back(m);
    }
    return lista;
}

QVariantMap AppBackend::agendarRestauracao(const QString &caminho)
{
    QVariantMap out;
    const bool ok = m_backupService.agendarRestauracao(caminho);
    out[QStringLiteral("ok")] = ok;
    out[QStringLiteral("erro")] = ok ? QString() : m_backupService.ultimoErro();
    return out;
}

QVariantMap AppBackend::statusBackup()
{
    QVariantMap out;
    const auto backups = m_backupService.listarBackups();
    out[QStringLiteral("total")] = backups.size();
    out[QStringLiteral("pasta")] = m_backupService.diretorio();
    if (!backups.isEmpty()) {
        out[QStringLiteral("ultimoCriadoEm")] = backups.first().criadoEm;
        out[QStringLiteral("ultimoResumo")] = backups.first().resumo;
    } else {
        out[QStringLiteral("ultimoCriadoEm")] = QString();
        out[QStringLiteral("ultimoResumo")] = QString();
    }
    return out;
}

// -------------------------------------------------- Relatório do celular

QVariantMap AppBackend::gerarRelatorioCelular()
{
    QVariantMap out;
    QString caminho;
    if (!m_relatorioMobile.gerar(&caminho)) {
        out[QStringLiteral("ok")] = false;
        out[QStringLiteral("erro")] = m_relatorioMobile.ultimoErro();
        return out;
    }
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("caminho")] = caminho;
    out[QStringLiteral("pasta")] = m_relatorioMobile.diretorio();
    out[QStringLiteral("erro")] = QString();
    return out;
}

QVariantMap AppBackend::statusRelatorioCelular()
{
    QVariantMap out;
    const QString arq = m_relatorioMobile.caminhoArquivo();
    const QFileInfo fi(arq);
    out[QStringLiteral("pasta")] = m_relatorioMobile.diretorio();
    out[QStringLiteral("caminho")] = arq;
    out[QStringLiteral("existe")] = fi.exists();
    out[QStringLiteral("atualizadoEm")] =
        fi.exists() ? fi.lastModified().toString(Qt::ISODate) : QString();
    return out;
}

QString AppBackend::formatarDinheiro(qlonglong centavos) const
{
    return Money::format(centavos);
}

QString AppBackend::formatarValor(qlonglong centavos) const
{
    return Money::formatPlain(centavos);
}

qlonglong AppBackend::parseDinheiro(const QString &texto) const
{
    const auto v = Money::parse(texto);
    return v ? *v : -1;
}
