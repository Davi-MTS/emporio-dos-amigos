#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Envio do resumo do negócio para o celular dos donos via bot do Telegram.
//
// Por que Telegram: chega como NOTIFICAÇÃO (não depende de alguém lembrar de
// abrir uma pasta), funciona de qualquer lugar, é gratuito e não exige servidor
// próprio — só uma chamada HTTPS para a API do Telegram.
//
// O token do bot e o chat de destino ficam em QSettings (máquina local), NUNCA
// no repositório: são credenciais do dono.
class TelegramService : public QObject
{
    Q_OBJECT

public:
    explicit TelegramService(QObject *parent = nullptr);

    bool configurado() const;
    QString token() const;
    QString chatId() const;
    bool ativo() const;                 // envio automático ligado?
    // Enviar também a CÓPIA DO BANCO ao fechar o caixa. É o que faz o backup
    // sair do computador: sem isto, um HD queimado leva os dados junto.
    bool enviaBackup() const;

    void salvarConfig(const QString &token, const QString &chatId, bool ativo,
                      bool enviaBackup);

    // Envia texto (HTML simples do Telegram: <b>, <i>, <code>). Assíncrono:
    // nunca trava a UI nem faz o fechamento de caixa falhar.
    void enviarMensagem(const QString &texto);

    // Envia um arquivo (relatório HTML ou a cópia do banco) como documento.
    // A API do Telegram recusa acima de 50 MB — arquivos maiores são ignorados
    // com aviso, em vez de falharem silenciosamente.
    void enviarArquivo(const QString &caminho, const QString &legenda);

    // Descobre o chat/grupo sozinho: lê as últimas mensagens recebidas pelo bot
    // (getUpdates) e devolve o id do chat mais recente. Evita depender de bots
    // de terceiros — o @userinfobot, por exemplo, nem entra em grupos.
    // Recebe o token por parâmetro para funcionar antes de salvar a config.
    void descobrirChat(const QString &token);

signals:
    // Chat encontrado pelo descobrirChat (nome = título do grupo ou da pessoa).
    void chatDescoberto(const QString &chatId, const QString &nome);
    // ok=false traz a mensagem de erro para a interface mostrar.
    void resultado(bool ok, const QString &mensagem);

private:
    QNetworkAccessManager *m_net;
};
