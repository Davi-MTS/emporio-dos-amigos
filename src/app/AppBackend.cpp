#include "app/AppBackend.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>

#include "utils/Money.h"

#include <utility>

#include <QFileInfo>
#include <QUrl>
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
    , m_loteRepo(m_db)
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
    , m_telegram(this)
    , m_produtosModel(new ProdutosListModel(this))
    , m_estoqueModel(new EstoqueListModel(this))
    , m_usuariosModel(new UsuariosListModel(this))
    , m_fornecedoresModel(new FornecedoresListModel(this))
    , m_comprasModel(new ComprasListModel(this))
    , m_clientesModel(new ClientesListModel(this))
    , m_contasPagarModel(new ContasPagarModel(this))
    , m_contasReceberModel(new ContasReceberModel(this))
    , m_vendasModel(new VendasListModel(this))
{
    connect(&m_telegram, &TelegramService::resultado,
            this, &AppBackend::telegramResultado);
    connect(&m_telegram, &TelegramService::chatDescoberto,
            this, &AppBackend::telegramChatDescoberto);
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

// Erros do SQLite chegavam crus na tela. Aqui viram frase de gente — e, quando
// não são conhecidos, a mensagem diz o que fazer em vez de despejar jargão.
static QString mensagemAmigavel(const QString &tecnico)
{
    if (tecnico.isEmpty())
        return tecnico;

    // Mensagem já escrita para humano (as do próprio sistema) passa direto.
    if (!tecnico.contains(QStringLiteral("constraint failed"))
        && !tecnico.contains(QStringLiteral("Unable to"))
        && !tecnico.contains(QStringLiteral("database is locked"))
        && !tecnico.contains(QStringLiteral("disk I/O")))
        return tecnico;

    if (tecnico.contains(QStringLiteral("UNIQUE constraint failed"))) {
        if (tecnico.contains(QStringLiteral("codigo_barras")))
            return AppBackend::tr("Este código de barras já está cadastrado em outro produto. "
                                  "Bipe o produto na busca para encontrá-lo.");
        if (tecnico.contains(QStringLiteral("categorias.nome")))
            return AppBackend::tr("Já existe uma categoria com esse nome.");
        if (tecnico.contains(QStringLiteral("usuarios.login")))
            return AppBackend::tr("Já existe um usuário com esse login.");
        return AppBackend::tr("Esse valor já está cadastrado em outro registro.");
    }
    if (tecnico.contains(QStringLiteral("FOREIGN KEY constraint failed")))
        return AppBackend::tr("Não dá para fazer isso: este registro está ligado a outros "
                              "(venda, compra ou movimentação de estoque).");
    if (tecnico.contains(QStringLiteral("NOT NULL constraint failed")))
        return AppBackend::tr("Falta preencher um campo obrigatório.");
    if (tecnico.contains(QStringLiteral("database is locked")))
        return AppBackend::tr("O banco está ocupado por um instante. Tente de novo.");
    if (tecnico.contains(QStringLiteral("disk I/O")))
        return AppBackend::tr("Não consegui gravar no disco. Verifique o espaço livre "
                              "e se o antivírus não está bloqueando a pasta.");

    return AppBackend::tr("Não foi possível concluir. Detalhe técnico no registro do sistema.");
}

QString AppBackend::ultimoErro() const
{
    const QString amigavel = mensagemAmigavel(m_erro);
    if (amigavel != m_erro && !m_erro.isEmpty())
        LogService::registrar(QStringLiteral("Erro técnico: %1").arg(m_erro));
    return amigavel;
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

    // Quanto ainda cabe no limite combinado — a pergunta do balcão é essa,
    // não "qual o limite". Limite 0 = sem limite definido.
    const qint64 disponivel = c->limiteFiado > 0
                                  ? qMax(Q_INT64_C(0), c->limiteFiado - c->saldoDevedor)
                                  : -1;
    m[QStringLiteral("limiteDisponivel")] = static_cast<qlonglong>(disponivel);

    const auto h = m_clienteRepo.historicoFiado(id);
    m[QStringLiteral("ultimaCompraFiado")] = h.ultimaCompra;
    m[QStringLiteral("ultimoPagamento")] = h.ultimoPagamento;
    m[QStringLiteral("contasAbertas")] = h.contasAbertas;
    m[QStringLiteral("vencimentoMaisAntigo")] = h.vencimentoMaisAntigo;
    return m;
}

QVariantMap AppBackend::resumoFiado()
{
    const auto r = m_clienteRepo.resumoFiado();
    QVariantMap m;
    m[QStringLiteral("total")] = static_cast<qlonglong>(r.total);
    m[QStringLiteral("atrasado")] = static_cast<qlonglong>(r.atrasado);
    m[QStringLiteral("quantosDevem")] = r.quantosDevem;
    m[QStringLiteral("quantosAtrasados")] = r.quantosAtrasados;
    m[QStringLiteral("acimaDoLimite")] = r.acimaDoLimite;
    m[QStringLiteral("maiorDevedorNome")] = r.maiorDevedorNome;
    m[QStringLiteral("maiorDevedorValor")] = static_cast<qlonglong>(r.maiorDevedorValor);
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
    m_contasPagarModel->setContas(m_financeiroRepo.contasPagar(!m_mostrarContasPagas));
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
    if (!m_financeiroRepo.pagar(id, forma)) {
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

void AppBackend::mostrarContasPagas(bool mostrar)
{
    m_mostrarContasPagas = mostrar;
    recarregarFinanceiro();
}

QVariantMap AppBackend::efeitoDoPagamento(const QString &forma, qlonglong valor)
{
    QVariantMap out;
    const bool emDinheiro = (forma == QStringLiteral("dinheiro"));
    const bool caixaAberto = m_sessaoId > 0;
    out[QStringLiteral("caixaAberto")] = caixaAberto;

    if (!emDinheiro) {
        out[QStringLiteral("sai")] = tr("Sai da sua conta no banco. Não mexe na gaveta do caixa.");
        out[QStringLiteral("alerta")] = QString();
        return out;
    }

    if (!caixaAberto) {
        // O pagamento é registrado, mas sem sessão aberta não há como lançar a
        // sangria: o dinheiro sairia da gaveta sem aparecer na conferência.
        out[QStringLiteral("sai")] = tr("Sai da gaveta do caixa.");
        out[QStringLiteral("alerta")] =
            tr("O caixa está FECHADO. A conta é quitada, mas a saída não entra na "
               "conferência de nenhum turno. Abra o caixa antes, se o dinheiro "
               "sair da gaveta agora.");
        return out;
    }

    const ResumoCaixa r = m_caixaRepo.resumo(m_sessaoId);
    const qint64 agora = r.dinheiroEsperado();
    out[QStringLiteral("sai")] = tr("Sai da gaveta do caixa, como sangria.");
    out[QStringLiteral("gavetaAgora")] = static_cast<qlonglong>(agora);
    out[QStringLiteral("gavetaDepois")] = static_cast<qlonglong>(agora - valor);
    out[QStringLiteral("alerta")] = (agora - valor) < 0
        ? tr("A gaveta não tem esse valor: ela ficaria negativa. Confira antes de confirmar.")
        : QString();
    return out;
}

QVariantMap AppBackend::estornarPagamento(int id)
{
    QVariantMap out;
    out[QStringLiteral("ok")] = false;

    if (!temPermissao(QStringLiteral("ve_financeiro"))) {
        out[QStringLiteral("erro")] = tr("Seu usuário não pode estornar pagamentos.");
        m_erro = out.value(QStringLiteral("erro")).toString();
        return out;
    }

    if (!m_db.transaction()) {
        out[QStringLiteral("erro")] = m_db.lastError().text();
        return out;
    }

    QString forma;
    QString descricao;
    qint64 valor = 0;
    if (!m_financeiroRepo.estornarPagamento(id, &forma, &valor, &descricao)) {
        m_db.rollback();
        out[QStringLiteral("erro")] = m_financeiroRepo.ultimoErro();
        m_erro = m_financeiroRepo.ultimoErro();
        return out;
    }

    // Se o dinheiro saiu da gaveta, ele volta para a gaveta — no caixa ABERTO
    // agora, que é onde a nota vai aparecer. Estornar um pagamento de ontem não
    // reabre o turno de ontem: o dinheiro entra hoje, com o motivo escrito.
    QString aviso;
    if (forma == QStringLiteral("dinheiro") && valor > 0) {
        if (m_sessaoId > 0) {
            if (!m_caixaRepo.registrarMovimento(m_sessaoId, QStringLiteral("suprimento"), valor,
                                                tr("Estorno de pagamento: %1").arg(descricao),
                                                m_usuarioId)) {
                m_db.rollback();
                out[QStringLiteral("erro")] = m_caixaRepo.ultimoErro();
                m_erro = m_caixaRepo.ultimoErro();
                return out;
            }
            aviso = tr("O dinheiro voltou para a gaveta do caixa aberto.");
        } else {
            aviso = tr("A conta foi reaberta, mas o caixa está fechado: o dinheiro "
                       "NÃO voltou para nenhuma gaveta. Lance um suprimento quando abrir.");
        }
    } else if (!forma.isEmpty()) {
        aviso = tr("O pagamento foi por %1 — desfaça também no banco, se já tiver saído.")
                    .arg(forma);
    } else {
        aviso = tr("Não havia registro de como esta conta foi paga, então o caixa "
                   "não foi tocado.");
    }

    if (!m_db.commit()) {
        m_db.rollback();
        out[QStringLiteral("erro")] = m_db.lastError().text();
        return out;
    }

    recarregarFinanceiro();
    m_erro.clear();
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("aviso")] = aviso;
    out[QStringLiteral("erro")] = QString();
    return out;
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

QVariantList AppBackend::produtosParaOrigemDose(int excluirId)
{
    QVariantList lista;
    for (const Produto &p : m_produtoRepo.listar()) {
        if (p.composto || p.doseDeProdutoId > 0 || p.id == excluirId)
            continue;   // dose de dose, ou de si mesmo, não faz sentido
        QVariantMap m;
        m[QStringLiteral("id")] = p.id;
        m[QStringLiteral("nome")] = p.nome;
        m[QStringLiteral("unidadeBase")] = p.unidadeBase;
        m[QStringLiteral("estoque")] = static_cast<qlonglong>(p.quantidadeEstoque);
        lista.push_back(m);
    }
    return lista;
}

int AppBackend::criarCategoria(const QString &nome)
{
    if (!temPermissao(QStringLiteral("edita_produto"))) {
        m_erro = tr("Seu usuário não pode criar categorias.");
        return 0;
    }
    const int id = m_produtoRepo.criarCategoria(nome);
    if (id <= 0) {
        m_erro = m_produtoRepo.ultimoErro();
        return 0;
    }
    m_erro.clear();
    return id;
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
    mapa[QStringLiteral("doseDeProdutoId")] = p->doseDeProdutoId;
    mapa[QStringLiteral("doseQuantidade")] = static_cast<qlonglong>(p->doseQuantidade);
    mapa[QStringLiteral("doseOrigemNome")] = p->doseOrigemNome;
    mapa[QStringLiteral("doseOrigemUnidade")] = p->doseOrigemUnidade;

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
    mapa[QStringLiteral("doseDeProdutoId")] = 0;
    mapa[QStringLiteral("doseQuantidade")] = 0;
    mapa[QStringLiteral("doseOrigemNome")] = QString();
    mapa[QStringLiteral("doseOrigemUnidade")] = QString();
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
    // A tela já desabilita os botões, mas a trava real fica aqui: assim vale
    // para qualquer caminho e não depende de a UI ter lembrado de conferir.
    if (!temPermissao(QStringLiteral("edita_produto"))) {
        m_erro = tr("Seu usuário não pode cadastrar ou alterar produtos.");
        return false;
    }

    Produto p;
    p.id = dados.value(QStringLiteral("id")).toInt();
    p.nome = dados.value(QStringLiteral("nome")).toString();
    p.categoriaId = dados.value(QStringLiteral("categoriaId")).toInt();
    p.unidadeBase = dados.value(QStringLiteral("unidadeBase"), QStringLiteral("unidade")).toString();
    p.estoqueMinimo = dados.value(QStringLiteral("estoqueMinimo")).toInt();
    p.localizacao = dados.value(QStringLiteral("localizacao")).toString();
    p.composto = dados.value(QStringLiteral("composto")).toBool();
    p.doseDeProdutoId = dados.value(QStringLiteral("doseDeProdutoId")).toInt();
    p.doseQuantidade = dados.value(QStringLiteral("doseQuantidade")).toLongLong();

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
    if (!temPermissao(QStringLiteral("edita_produto"))) {
        m_erro = tr("Seu usuário não pode inativar produtos.");
        return false;
    }
    if (!m_produtoRepo.inativar(id)) {
        m_erro = m_produtoRepo.ultimoErro();
        return false;
    }
    m_erro.clear();
    recarregarProdutos();
    recarregarEstoque();
    return true;
}

QVariantMap AppBackend::definirFotoProduto(int produtoId, const QString &caminhoArquivo)
{
    QVariantMap out;
    out[QStringLiteral("ok")] = false;

    if (!temPermissao(QStringLiteral("edita_produto"))) {
        out[QStringLiteral("erro")] = tr("Seu usuário não pode alterar produtos.");
        return out;
    }
    if (produtoId <= 0) {
        out[QStringLiteral("erro")] = tr("Salve o produto antes de adicionar a foto.");
        return out;
    }

    QImageReader leitor(caminhoArquivo);
    leitor.setAutoTransform(true);   // respeita a rotação da foto do celular
    const QImage original = leitor.read();
    if (original.isNull()) {
        out[QStringLiteral("erro")] = tr("Não consegui ler a imagem (%1).").arg(leitor.errorString());
        return out;
    }

    // Reduz antes de gravar: a foto é só para reconhecer o produto na tela, e
    // o banco inteiro viaja no backup. 320 px no lado maior resolve os dois.
    const int kLadoMaximo = 320;
    QImage reduzida = original;
    if (original.width() > kLadoMaximo || original.height() > kLadoMaximo) {
        reduzida = original.scaled(kLadoMaximo, kLadoMaximo,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray jpeg;
    {
        QBuffer buffer(&jpeg);
        buffer.open(QIODevice::WriteOnly);
        if (!reduzida.save(&buffer, "JPEG", 80)) {
            out[QStringLiteral("erro")] = tr("Não consegui converter a imagem.");
            return out;
        }
    }

    if (!m_produtoRepo.salvarFoto(produtoId, jpeg)) {
        out[QStringLiteral("erro")] = m_produtoRepo.ultimoErro();
        m_erro = out.value(QStringLiteral("erro")).toString();
        return out;
    }

    ++m_versaoFotos;
    Q_EMIT versaoFotosChanged();
    recarregarProdutos();
    m_erro.clear();
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("erro")] = QString();
    out[QStringLiteral("bytes")] = jpeg.size();
    return out;
}

bool AppBackend::removerFotoProduto(int produtoId)
{
    if (!temPermissao(QStringLiteral("edita_produto"))) {
        m_erro = tr("Seu usuário não pode alterar produtos.");
        return false;
    }
    if (!m_produtoRepo.removerFoto(produtoId)) {
        m_erro = m_produtoRepo.ultimoErro();
        return false;
    }
    ++m_versaoFotos;
    Q_EMIT versaoFotosChanged();
    recarregarProdutos();
    m_erro.clear();
    return true;
}

bool AppBackend::produtoTemFoto(int produtoId)
{
    return !m_produtoRepo.foto(produtoId).isEmpty();
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
    // Dose não tem estoque próprio: o que existe é o que a garrafa ainda dá.
    // Ex.: 700 ml na garrafa, dose de 50 ml => 14 doses.
    const auto p = m_produtoRepo.obter(produtoId);
    if (p && p->doseDeProdutoId > 0 && p->doseQuantidade > 0) {
        const qint64 naOrigem = m_estoqueRepo.item(p->doseDeProdutoId).quantidade;
        return static_cast<qlonglong>(naOrigem / p->doseQuantidade);
    }
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
                                  const QString &custoTexto, const QString &observacao,
                                  const QString &validade, const QString &codigoLote)
{
    if (!temPermissao(QStringLiteral("recebe_mercadoria"))) {
        m_erro = tr("Seu usuário não pode dar entrada de mercadoria.");
        return false;
    }

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

    if (!m_estoqueRepo.registrarEntradaMilli(produtoId, qtdBase, custoUnitBaseMilli,
                                             m_usuarioId, observacao)) {
        m_erro = m_estoqueRepo.ultimoErro();
        return false;
    }

    // Validade é opcional: a maioria dos produtos da distribuidora não vence em
    // prazo curto. Informada, vira um lote e passa a ser cobrada na tela de
    // Vencimento. Se o lote falhar, a entrada NÃO é desfeita — a mercadoria
    // realmente entrou; o aviso vai para o log.
    if (!validade.trimmed().isEmpty()) {
        if (!m_loteRepo.registrar(produtoId, qtdBase, validade.trimmed(), codigoLote))
            qWarning("Entrada gravada, mas o lote falhou: %s",
                     qUtf8Printable(m_loteRepo.ultimoErro()));
    }

    m_erro.clear();
    recarregarEstoque();
    recarregarProdutos();
    return true;
}

QVariantList AppBackend::lotes(int dias)
{
    QVariantList lista;
    for (const Lote &l : m_loteRepo.listar(dias)) {
        QVariantMap m;
        m[QStringLiteral("id")] = l.id;
        m[QStringLiteral("produtoId")] = l.produtoId;
        m[QStringLiteral("produto")] = l.produtoNome;
        m[QStringLiteral("unidade")] = l.unidadeBase;
        m[QStringLiteral("codigo")] = l.codigo;
        m[QStringLiteral("validade")] = l.validade;
        m[QStringLiteral("quantidade")] = static_cast<qlonglong>(l.quantidade);
        m[QStringLiteral("dias")] = l.diasParaVencer;
        lista.push_back(m);
    }
    return lista;
}

QVariantMap AppBackend::resumoVencimento()
{
    const ResumoVencimento r = m_loteRepo.resumo();
    QVariantMap m;
    m[QStringLiteral("vencidos")] = r.vencidos;
    m[QStringLiteral("venceEm7")] = r.venceEm7;
    m[QStringLiteral("venceEm30")] = r.venceEm30;
    m[QStringLiteral("quantidadeVencida")] = static_cast<qlonglong>(r.quantidadeVencida);
    return m;
}

QVariantList AppBackend::divergenciasDeLote()
{
    QVariantList lista;
    for (const auto &d : m_loteRepo.divergencias()) {
        QVariantMap m;
        m[QStringLiteral("produto")] = d.first;
        m[QStringLiteral("diferenca")] = static_cast<qlonglong>(d.second);
        lista.push_back(m);
    }
    return lista;
}

bool AppBackend::registrarInventario(int produtoId, int novaQtdBase, const QString &motivo)
{
    // Ajuste de inventário reescreve o saldo sem nota nenhuma: é por onde some
    // mercadoria sem deixar rastro. Só quem tem "ajusta_estoque".
    if (!temPermissao(QStringLiteral("ajusta_estoque"))) {
        m_erro = tr("Seu usuário não pode ajustar o estoque por inventário.");
        return false;
    }

    if (!m_estoqueRepo.registrarInventario(produtoId, novaQtdBase, motivo, m_usuarioId)) {
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
    // Retirada tira mercadoria fora da venda (quebra, consumo, brinde).
    if (!temPermissao(QStringLiteral("ajusta_estoque"))) {
        m_erro = tr("Seu usuário não pode registrar retirada de estoque.");
        return false;
    }

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

    if (!m_estoqueRepo.registrarSaida(produtoId, qtdBase, motivo, m_usuarioId)) {
        m_erro = m_estoqueRepo.ultimoErro();
        return false;
    }
    // Quebra/consumo também tira de uma remessa: sem isto a tela de Vencimento
    // continuaria cobrando mercadoria que não está mais na prateleira.
    m_loteRepo.consumirFefo(produtoId, qtdBase);

    m_erro.clear();
    recarregarEstoque();
    recarregarProdutos();
    return true;
}

// ---------------------------------------------------------------- PDV / Vendas

bool AppBackend::abrirCaixa(const QString &valorAberturaTexto)
{
    // Campo vazio = abre sem troco, o que é legítimo. Mas texto que NÃO é um
    // valor virava R$ 0,00 em silêncio: quem digitou "1OO" (letra O) abria o
    // caixa zerado e só descobria no fechamento, com uma diferença de 100 reais
    // que ninguém sabia explicar.
    qint64 valor = 0;
    const QString t = valorAberturaTexto.trimmed();
    if (!t.isEmpty()) {
        const auto v = Money::parse(t);
        if (!v || *v < 0) {
            m_erro = tr("Troco inicial inválido. Escreva só o valor, como 100,00 "
                        "(ou deixe vazio para abrir sem troco).");
            return false;
        }
        valor = *v;
    }
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
    m[QStringLiteral("doseDeProdutoId")] = p.doseDeProdutoId;
    m[QStringLiteral("doseQuantidade")] = static_cast<qlonglong>(p.doseQuantidade);
    m[QStringLiteral("doseOrigemNome")] = p.doseOrigemNome;
    m[QStringLiteral("temFoto")] = p.temFoto;
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

        // Dose: a origem é sempre a mesma garrafa, então quem resolve é o
        // backend — a tela vende a dose como qualquer outro produto, com um
        // bipe, sem diálogo de escolha.
        if (l.insumos.isEmpty()) {
            const auto pr = m_produtoRepo.obter(l.produtoId);
            if (pr && pr->doseDeProdutoId > 0 && pr->doseQuantidade > 0) {
                InsumoResolvido ir;
                ir.produtoId = pr->doseDeProdutoId;
                ir.quantidade = pr->doseQuantidade;
                l.insumos.push_back(ir);
            }
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

    // Desconto é dinheiro saindo do caixa por decisão de quem está no balcão.
    // Sem a permissão, a venda não passa — em vez de passar com o desconto
    // apagado em silêncio, que faria o operador cobrar errado sem entender.
    if (!temPermissao(QStringLiteral("pode_dar_desconto"))) {
        qint64 descontoItens = 0;
        for (const LinhaVenda &l : itens)
            descontoItens += l.desconto;
        if (desconto > 0 || descontoItens > 0) {
            QVariantMap out;
            out[QStringLiteral("ok")] = false;
            out[QStringLiteral("erro")] = tr("Seu usuário não pode dar desconto. "
                                             "Chame o responsável.");
            m_erro = out.value(QStringLiteral("erro")).toString();
            return out;
        }
    }

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

void AppBackend::recarregarVendas(int dias)
{
    m_vendasModel->setVendas(m_vendaRepo.listar(dias));
}

QVariantList AppBackend::itensDaVenda(int vendaId)
{
    QVariantList lista;
    for (const ItemVendido &it : m_vendaRepo.itens(vendaId)) {
        QVariantMap m;
        m[QStringLiteral("produto")] = it.produto;
        m[QStringLiteral("embalagem")] = it.embalagem;
        m[QStringLiteral("qtd")] = static_cast<qlonglong>(it.qtdBase);
        m[QStringLiteral("precoUnit")] = static_cast<qlonglong>(it.precoUnit);
        m[QStringLiteral("desconto")] = static_cast<qlonglong>(it.desconto);
        lista.push_back(m);
    }
    return lista;
}

QVariantMap AppBackend::cancelarVenda(int vendaId, const QString &motivo)
{
    QVariantMap out;
    if (!temPermissao(QStringLiteral("pode_cancelar_venda"))) {
        out[QStringLiteral("ok")] = false;
        out[QStringLiteral("erro")] = QStringLiteral("Você não tem permissão para cancelar vendas.");
        return out;
    }
    if (motivo.trimmed().isEmpty()) {
        out[QStringLiteral("ok")] = false;
        out[QStringLiteral("erro")] = QStringLiteral("Informe o motivo do cancelamento.");
        return out;
    }
    const bool ok = m_vendaRepo.cancelarVenda(vendaId, motivo.trimmed(), m_usuarioId, m_sessaoId);
    LogService::registrar(QStringLiteral("Venda #%1 %2 por '%3' — motivo: %4")
                              .arg(vendaId)
                              .arg(ok ? QStringLiteral("CANCELADA") : QStringLiteral("falha ao cancelar"),
                                   m_usuarioAtual.value(QStringLiteral("nome")).toString(),
                                   motivo.trimmed()));
    out[QStringLiteral("ok")] = ok;
    out[QStringLiteral("erro")] = ok ? QString() : m_vendaRepo.ultimoErro();
    if (ok) {
        recarregarEstoque();
        recarregarProdutos();
        recarregarClientes();
        recarregarFinanceiro();
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
    // A tela já barra, mas o fechamento é a operação mais sensível do sistema:
    // um valor ilegível NÃO pode virar "contei zero" por omissão.
    const auto v = Money::parse(dinheiroContadoTexto);
    if (!v || *v < 0) {
        QVariantMap erro;
        erro[QStringLiteral("ok")] = false;
        erro[QStringLiteral("erro")] = tr("Valor contado inválido. Escreva só o valor, "
                                          "como 250,00.");
        m_erro = erro.value(QStringLiteral("erro")).toString();
        return erro;
    }
    const qint64 contado = *v;
    const ResultadoFechamento r = m_caixaRepo.fechar(m_sessaoId, contado, m_usuarioId);

    QVariantMap out;
    out[QStringLiteral("ok")] = r.ok;
    out[QStringLiteral("esperado")] = static_cast<qlonglong>(r.esperado);
    out[QStringLiteral("informado")] = static_cast<qlonglong>(r.informado);
    out[QStringLiteral("diferenca")] = static_cast<qlonglong>(r.diferenca);
    out[QStringLiteral("erro")] = r.erro;
    if (r.ok) {
        LogService::registrar(QStringLiteral("Caixa fechado por '%1' — esperado %2, contado %3, diferença %4")
                                  .arg(m_usuarioAtual.value(QStringLiteral("nome")).toString(),
                                       Money::format(r.esperado), Money::format(r.informado),
                                       Money::format(r.diferenca)));
        m_sessaoId = 0;
        emit caixaAbertoChanged();
        // Backup automático de fim de expediente (melhor esforço — nunca faz o
        // fechamento do caixa falhar). Mantém as 5 cópias mais recentes.
        BackupInfo backup;
        const bool temBackup = m_backupService.criarBackup(&backup);
        if (temBackup)
            m_backupService.rotacionar(5);
        // Atualiza o relatório completo (arquivo local anexado no Telegram).
        m_relatorioMobile.gerar(nullptr);
        // E manda para o celular dos donos (chega como notificação).
        if (m_telegram.configurado() && m_telegram.ativo()) {
            m_telegram.enviarMensagem(m_relatorioMobile.resumoTexto());
            m_telegram.enviarArquivo(m_relatorioMobile.caminhoArquivo(),
                                     QStringLiteral("Relatório completo"));
            // A CÓPIA DO BANCO vai junto: é o que tira o backup de dentro do PC.
            // Sem isto, um HD queimado ou um roubo levam os dados junto.
            if (temBackup && m_telegram.enviaBackup()) {
                m_telegram.enviarArquivo(
                    backup.caminho,
                    QStringLiteral("Backup do sistema — %1").arg(backup.resumo));
                LogService::registrar(QStringLiteral("Backup enviado ao Telegram: %1")
                                          .arg(backup.caminho));
            }
        }
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
    // Backup e relatório do celular andam juntos (mesma "foto" dos dados).
    m_relatorioMobile.gerar(nullptr);
    // Manda a cópia para fora do PC também no botão manual.
    if (m_telegram.configurado() && m_telegram.ativo() && m_telegram.enviaBackup())
        m_telegram.enviarArquivo(info.caminho,
                                 QStringLiteral("Backup do sistema — %1").arg(info.resumo));
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


QVariantMap AppBackend::conferirArquivoBackup(const QString &caminho)
{
    QVariantMap out;
    BackupInfo info;
    const bool ok = m_backupService.validarArquivoBackup(caminho, &info);
    out[QStringLiteral("ok")] = ok;
    out[QStringLiteral("erro")] = ok ? QString() : m_backupService.ultimoErro();
    out[QStringLiteral("resumo")] = info.resumo;
    out[QStringLiteral("tamanho")] = static_cast<qlonglong>(info.tamanho);
    out[QStringLiteral("criadoEm")] = info.criadoEm;
    return out;
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

// ------------------------------------------------------------------ Telegram

QVariantMap AppBackend::configTelegram()
{
    QVariantMap m;
    m[QStringLiteral("token")] = m_telegram.token();
    m[QStringLiteral("chatId")] = m_telegram.chatId();
    m[QStringLiteral("ativo")] = m_telegram.ativo();
    m[QStringLiteral("enviaBackup")] = m_telegram.enviaBackup();
    m[QStringLiteral("configurado")] = m_telegram.configurado();
    return m;
}

void AppBackend::salvarConfigTelegram(const QString &token, const QString &chatId,
                                      bool ativo, bool enviaBackup)
{
    m_telegram.salvarConfig(token, chatId, ativo, enviaBackup);
}

void AppBackend::testarTelegram()
{
    if (!m_telegram.configurado()) {
        emit telegramResultado(false, QStringLiteral("Informe o token e o chat antes de testar."));
        return;
    }
    m_telegram.enviarMensagem(m_relatorioMobile.resumoTexto());
}

void AppBackend::descobrirChatTelegram(const QString &token)
{
    m_telegram.descobrirChat(token);
}

// ----------------------------------------------------------- Registro (log)

QVariantMap AppBackend::statusLog()
{
    QVariantMap m;
    const QString arq = LogService::caminhoArquivo();
    m[QStringLiteral("pasta")] = LogService::pasta();
    m[QStringLiteral("arquivo")] = arq;
    m[QStringLiteral("tamanho")] = static_cast<qlonglong>(QFileInfo(arq).size());
    m[QStringLiteral("url")] = QUrl::fromLocalFile(LogService::pasta()).toString();
    return m;
}

QStringList AppBackend::ultimasLinhasLog(int n)
{
    return LogService::ultimasLinhas(n);
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
