#pragma once

#include "domain/caixa/CaixaRepository.h"
#include "domain/clientes/ClienteRepository.h"
#include "domain/compras/CompraRepository.h"
#include "domain/compras/FornecedorRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/financeiro/FinanceiroRepository.h"
#include "domain/produtos/ProdutoRepository.h"
#include "domain/relatorios/RelatorioRepository.h"
#include "domain/usuarios/UsuarioRepository.h"
#include "domain/vendas/VendaRepository.h"
#include "services/backup/BackupService.h"
#include "services/relatoriomobile/RelatorioMobileService.h"
#include "services/log/LogService.h"
#include "services/telegram/TelegramService.h"
#include "models/ClientesListModel.h"
#include "models/ComprasListModel.h"
#include "models/ContasPagarModel.h"
#include "models/ContasReceberModel.h"
#include "models/EstoqueListModel.h"
#include "models/FornecedoresListModel.h"
#include "models/ProdutosListModel.h"
#include "models/UsuariosListModel.h"
#include "models/VendasListModel.h"

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

// Fachada exposta ao QML (via contextProperty "App"). Reúne os models e as
// operações que a UI precisa, mantendo o QML simples e sem SQL.
//
// Fica no núcleo (sem dependência de QML) para poder ser testado; o main.cpp
// apenas o instancia com a conexão do banco e o registra no contexto.
class AppBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProdutosListModel *produtos READ produtos CONSTANT)
    Q_PROPERTY(EstoqueListModel *estoque READ estoque CONSTANT)
    Q_PROPERTY(UsuariosListModel *usuarios READ usuarios CONSTANT)
    Q_PROPERTY(FornecedoresListModel *fornecedores READ fornecedores CONSTANT)
    Q_PROPERTY(ComprasListModel *compras READ compras CONSTANT)
    Q_PROPERTY(ClientesListModel *clientes READ clientes CONSTANT)
    Q_PROPERTY(ContasPagarModel *contasPagar READ contasPagar CONSTANT)
    Q_PROPERTY(ContasReceberModel *contasReceber READ contasReceber CONSTANT)
    Q_PROPERTY(VendasListModel *vendas READ vendas CONSTANT)
    Q_PROPERTY(bool caixaAberto READ caixaAberto NOTIFY caixaAbertoChanged)
    Q_PROPERTY(bool logado READ logado NOTIFY sessaoUsuarioChanged)
    Q_PROPERTY(bool precisaCriarAdmin READ precisaCriarAdmin NOTIFY sessaoUsuarioChanged)
    Q_PROPERTY(QVariantMap usuarioAtual READ usuarioAtual NOTIFY sessaoUsuarioChanged)

public:
    explicit AppBackend(QSqlDatabase db, QObject *parent = nullptr);

    ProdutosListModel *produtos() const { return m_produtosModel; }
    EstoqueListModel *estoque() const { return m_estoqueModel; }
    UsuariosListModel *usuarios() const { return m_usuariosModel; }
    FornecedoresListModel *fornecedores() const { return m_fornecedoresModel; }
    ComprasListModel *compras() const { return m_comprasModel; }
    ClientesListModel *clientes() const { return m_clientesModel; }
    ContasPagarModel *contasPagar() const { return m_contasPagarModel; }
    ContasReceberModel *contasReceber() const { return m_contasReceberModel; }
    VendasListModel *vendas() const { return m_vendasModel; }
    bool caixaAberto() const { return m_sessaoId > 0; }
    bool logado() const { return m_usuarioId > 0; }
    bool precisaCriarAdmin();
    QVariantMap usuarioAtual() const { return m_usuarioAtual; }

    // --- Produtos ---
    Q_INVOKABLE void recarregarProdutos(const QString &filtro = QString());
    Q_INVOKABLE QVariantList categorias();
    // Produtos que podem ser a ORIGEM de uma dose (a garrafa): ativos, não
    // compostos e que não sejam eles próprios uma dose.
    Q_INVOKABLE QVariantList produtosParaOrigemDose(int excluirId = 0);
    // Cria a categoria e devolve o id (0 em caso de erro). Exige edita_produto.
    Q_INVOKABLE int criarCategoria(const QString &nome);
    // Linhas da composição de um produto composto, com os produtos de cada
    // categoria para escolha na venda: [{categoriaId, categoriaNome, unidade,
    // quantidade, produtos:[{id,nome}]}].
    Q_INVOKABLE QVariantList composicaoParaVenda(int produtoId);
    Q_INVOKABLE QVariantMap produto(int id);       // completo, com embalagens
    Q_INVOKABLE QVariantMap novoProduto();         // modelo em branco p/ o formulário
    Q_INVOKABLE bool salvarProduto(const QVariantMap &dados);
    Q_INVOKABLE bool inativarProduto(int id);

    // Foto do produto. O arquivo escolhido é REDUZIDO antes de gravar (lado
    // maior 320 px, JPEG) — o banco inteiro sai da loja no backup do Telegram.
    // { ok, erro, bytes }. Exige edita_produto.
    Q_INVOKABLE QVariantMap definirFotoProduto(int produtoId, const QString &caminhoArquivo);
    Q_INVOKABLE bool removerFotoProduto(int produtoId);
    Q_INVOKABLE bool produtoTemFoto(int produtoId);
    // Muda a cada foto gravada: as telas põem isto na URL para o Qt não
    // devolver a imagem antiga do cache depois de trocar a foto.
    Q_PROPERTY(int versaoFotos READ versaoFotos NOTIFY versaoFotosChanged)
    int versaoFotos() const { return m_versaoFotos; }

    // --- Estoque ---
    Q_INVOKABLE void recarregarEstoque(const QString &filtro = QString());
    Q_INVOKABLE QVariantMap itemEstoque(int produtoId);
    Q_INVOKABLE QVariantList embalagensDe(int produtoId);
    // Saldo atual em estoque (unidade base). 0 se não houver linha. Usado pelo
    // PDV para avisar sobre venda com estoque insuficiente.
    Q_INVOKABLE qlonglong estoqueDisponivel(int produtoId);
    // qtdEmb embalagens de `embalagemId` (0 = unidade base). custoTexto vazio
    // mantém o custo médio atual.
    Q_INVOKABLE bool registrarEntrada(int produtoId, int embalagemId, int qtdEmb,
                                      const QString &custoTexto, const QString &observacao);
    Q_INVOKABLE bool registrarInventario(int produtoId, int novaQtdBase,
                                         const QString &motivo);
    // Retirada manual (perda/quebra/consumo): baixa qtdEmb embalagens do estoque.
    Q_INVOKABLE bool registrarRetirada(int produtoId, int embalagemId, int qtdEmb,
                                       const QString &motivo);

    // --- PDV / Caixa / Vendas ---
    Q_INVOKABLE bool abrirCaixa(const QString &valorAberturaTexto);
    // Busca por código de barras: retorna { encontrado, produtoId, nome,
    // embalagemId, embalagemNome, fator, preco, unidadeBase }.
    Q_INVOKABLE QVariantMap buscarProdutoPorCodigo(const QString &codigo);
    // Sugestões por nome (para o campo do PDV). Mesma forma do item acima.
    // incluirCompostos=false (usado na Compra) omite produtos compostos, que não
    // se compram — só se vendem.
    Q_INVOKABLE QVariantList buscarProdutosPorNome(const QString &termo,
                                                   bool incluirCompostos = true);
    // dados: { desconto, clienteId, itens:[{produtoId,embalagemId,fator,qtd,precoUnit,desconto}],
    //          pagamentos:[{forma,valor}] }. Retorna { ok, vendaId, total, troco, erro }.
    Q_INVOKABLE QVariantMap finalizarVenda(const QVariantMap &dados);

    // Resumo da sessão aberta (por forma, esperado etc.). { aberto:false } se fechado.
    // --- Histórico de vendas ---
    Q_INVOKABLE void recarregarVendas(int dias);
    // Itens de uma venda: [{produto, embalagem, qtd, precoUnit, desconto}].
    Q_INVOKABLE QVariantList itensDaVenda(int vendaId);
    // Cancela a venda (devolve estoque, cancela fiado, estorna dinheiro).
    // Exige permissão pode_cancelar_venda. { ok, erro }.
    Q_INVOKABLE QVariantMap cancelarVenda(int vendaId, const QString &motivo);

    Q_INVOKABLE QVariantMap caixaResumo();
    Q_INVOKABLE bool registrarSangria(const QString &valorTexto, const QString &motivo);
    Q_INVOKABLE bool registrarSuprimento(const QString &valorTexto, const QString &motivo);
    // Fecha o caixa. Retorna { ok, esperado, informado, diferenca, erro }.
    Q_INVOKABLE QVariantMap fecharCaixa(const QString &dinheiroContadoTexto);

    // --- Login / usuários / permissões ---
    Q_INVOKABLE bool login(const QString &login, const QString &senha);
    Q_INVOKABLE bool criarAdmin(const QString &nome, const QString &login, const QString &senha);
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool temPermissao(const QString &chave) const;
    Q_INVOKABLE void recarregarUsuarios();
    Q_INVOKABLE QVariantMap usuario(int id);
    Q_INVOKABLE QVariantMap novoUsuario();
    Q_INVOKABLE bool salvarUsuario(const QVariantMap &dados, const QString &senha);
    Q_INVOKABLE bool inativarUsuario(int id);
    Q_INVOKABLE QVariantList perfis();

    // --- Compras / fornecedores ---
    Q_INVOKABLE void recarregarFornecedores(const QString &filtro = QString());
    Q_INVOKABLE QVariantList fornecedoresLista();          // {id, nome} para combos
    Q_INVOKABLE QVariantMap fornecedor(int id);
    Q_INVOKABLE QVariantMap novoFornecedor();
    Q_INVOKABLE bool salvarFornecedor(const QVariantMap &dados);
    Q_INVOKABLE void recarregarCompras();
    // dados: { fornecedorId, gerarContaPagar, vencimento,
    //          itens:[{produtoId,embalagemId,fator,qtd,custo}] }.
    // Retorna { ok, compraId, total, erro }.
    Q_INVOKABLE QVariantMap registrarCompra(const QVariantMap &dados);

    // --- Clientes / fiado ---
    Q_INVOKABLE void recarregarClientes(const QString &filtro = QString());
    Q_INVOKABLE QVariantList clientesLista();       // {id, nome, saldo, limite} p/ o PDV
    Q_INVOKABLE QVariantMap cliente(int id);
    // Painel do fiado na tela de Clientes (era preciso ir ao Dashboard/Relatórios).
    Q_INVOKABLE QVariantMap resumoFiado();
    Q_INVOKABLE QVariantMap novoCliente();
    Q_INVOKABLE bool salvarCliente(const QVariantMap &dados);
    Q_INVOKABLE bool inativarCliente(int id);
    // Recebe pagamento de fiado do cliente (parcial ou total), abatendo as contas
    // da mais antiga p/ a mais nova. valorTexto vazio = quita tudo. Se forma for
    // "dinheiro" e houver caixa aberto, lança o valor recebido na gaveta.
    // Retorna { ok, aplicado, erro }.
    Q_INVOKABLE QVariantMap receberDeCliente(int clienteId, const QString &valorTexto,
                                             const QString &forma);

    // --- Financeiro (contas a pagar / receber) ---
    Q_INVOKABLE void recarregarFinanceiro();
    Q_INVOKABLE QVariantMap resumoFinanceiro();
    // forma "dinheiro" (padrão) com caixa aberto lança uma SANGRIA (o dinheiro
    // sai da gaveta); outras formas só quitam a conta.
    Q_INVOKABLE bool pagarConta(int id, const QString &forma = QStringLiteral("dinheiro"));

    // Desfaz um pagamento lançado por engano: reabre a conta e, se tinha saído
    // da gaveta, devolve o dinheiro para o caixa aberto. { ok, erro, aviso }.
    Q_INVOKABLE QVariantMap estornarPagamento(int id);

    // Mostra as contas JÁ PAGAS junto com as abertas (para achar o que estornar).
    Q_INVOKABLE void mostrarContasPagas(bool mostrar);

    // Onde o dinheiro vai passar, em português, para a tela avisar ANTES de
    // confirmar. { sai, gavetaAgora, gavetaDepois, caixaAberto, alerta }.
    Q_INVOKABLE QVariantMap efeitoDoPagamento(const QString &forma, qlonglong valor);
    // Recebe uma conta a receber específica (parcial ou total). valorTexto vazio =
    // recebe o valor cheio da conta. forma "dinheiro" com caixa aberto entra na
    // gaveta. Retorna { ok, aplicado, erro }.
    Q_INVOKABLE QVariantMap receberContaValor(int id, const QString &valorTexto,
                                              const QString &forma);
    Q_INVOKABLE bool criarDespesa(const QString &descricao, const QString &valorTexto,
                                  const QString &vencimento);

    // --- Relatórios / dashboard (dias<=0 = hoje) ---
    Q_INVOKABLE QVariantMap dashboard();
    Q_INVOKABLE QVariantMap relatorioFaturamento(int dias);
    Q_INVOKABLE QVariantList relatorioFormas(int dias);
    Q_INVOKABLE QVariantList relatorioMaisVendidos(int dias, int limite);
    Q_INVOKABLE QVariantList relatorioProdutosParados(int dias);

    // --- Backup / restauração (acesso restrito ao Administrador na UI) ---
    // Cria um backup íntegro agora e aplica a retenção. { ok, caminho, resumo, erro }.
    Q_INVOKABLE QVariantMap fazerBackup();
    // Lista as cópias existentes (mais recente primeiro): [{caminho, criadoEm, tamanho, resumo}].
    Q_INVOKABLE QVariantList backupsDisponiveis();
    // Agenda restaurar a cópia informada no próximo início (com backup de emergência).
    // { ok, erro }. A UI deve avisar para fechar e reabrir o app.
    // Confere um arquivo escolhido à mão antes de oferecer a restauração.
    // { ok, erro, resumo, tamanho, criadoEm }
    Q_INVOKABLE QVariantMap conferirArquivoBackup(const QString &caminho);
    Q_INVOKABLE QVariantMap agendarRestauracao(const QString &caminho);
    // Situação: { total, ultimoCriadoEm, ultimoResumo, pasta }.
    Q_INVOKABLE QVariantMap statusBackup();

    // --- Relatório do celular (HTML local, enviado anexado no Telegram) ---
    // Gera/atualiza o relatório HTML. { ok, caminho, pasta, erro }.
    Q_INVOKABLE QVariantMap gerarRelatorioCelular();
    // Situação: { pasta, caminho, existe, atualizadoEm }.
    Q_INVOKABLE QVariantMap statusRelatorioCelular();

    // --- Telegram (resumo no celular dos donos; só Admin na UI) ---
    // { token, chatId, ativo, configurado }.
    Q_INVOKABLE QVariantMap configTelegram();
    Q_INVOKABLE void salvarConfigTelegram(const QString &token, const QString &chatId,
                                          bool ativo, bool enviaBackup);
    // Dispara um envio de teste; o retorno vem pelo sinal telegramResultado.
    Q_INVOKABLE void testarTelegram();

    // --- Registro do sistema (log em arquivo) ---
    // { pasta, arquivo, tamanho, url } — url serve p/ Qt.openUrlExternally.
    Q_INVOKABLE QVariantMap statusLog();
    Q_INVOKABLE QStringList ultimasLinhasLog(int n);
    // Descobre o chat/grupo automaticamente (resposta em telegramChatDescoberto).
    Q_INVOKABLE void descobrirChatTelegram(const QString &token);

    // --- Utilidades de dinheiro (centavos <-> texto pt-BR) ---
    Q_INVOKABLE QString formatarDinheiro(qlonglong centavos) const;      // "R$ 12,50"
    Q_INVOKABLE QString formatarValor(qlonglong centavos) const;         // "12,50"
    Q_INVOKABLE qlonglong parseDinheiro(const QString &texto) const;     // -1 se inválido

    Q_INVOKABLE QString ultimoErro() const { return m_erro; }

signals:
    void versaoFotosChanged();
    void caixaAbertoChanged();
    void sessaoUsuarioChanged();
    // Resposta do envio ao Telegram (assíncrono).
    void telegramResultado(bool ok, const QString &mensagem);
    void telegramChatDescoberto(const QString &chatId, const QString &nome);

private:
    void _definirUsuarioAtual(const Usuario &u);

    QSqlDatabase m_db;
    ProdutoRepository m_produtoRepo;
    EstoqueRepository m_estoqueRepo;
    CaixaRepository m_caixaRepo;
    VendaRepository m_vendaRepo;
    UsuarioRepository m_usuarioRepo;
    FornecedorRepository m_fornecedorRepo;
    CompraRepository m_compraRepo;
    ClienteRepository m_clienteRepo;
    FinanceiroRepository m_financeiroRepo;
    RelatorioRepository m_relatorioRepo;
    BackupService m_backupService;
    RelatorioMobileService m_relatorioMobile;
    TelegramService m_telegram;
    ProdutosListModel *m_produtosModel;
    EstoqueListModel *m_estoqueModel;
    UsuariosListModel *m_usuariosModel;
    FornecedoresListModel *m_fornecedoresModel;
    ComprasListModel *m_comprasModel;
    ClientesListModel *m_clientesModel;
    ContasPagarModel *m_contasPagarModel;
    ContasReceberModel *m_contasReceberModel;
    VendasListModel *m_vendasModel;
    QVariantMap m_usuarioAtual;
    int m_usuarioId = 0;
    int m_sessaoId = 0;
    int m_versaoFotos = 1;
    // Financeiro: por padrão a lista traz só o que está em aberto (é o que
    // importa no dia). Ligado, mostra também o que já foi pago — é como se
    // acha uma conta lançada por engano para estornar.
    bool m_mostrarContasPagas = false;
    QString m_erro;
};
