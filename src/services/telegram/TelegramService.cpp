#include "services/telegram/TelegramService.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr auto kChaveToken = "telegram/token";
constexpr auto kChaveChat  = "telegram/chatId";
constexpr auto kChaveAtivo = "telegram/ativo";
constexpr auto kChaveBackup = "telegram/enviaBackup";
constexpr qint64 kLimiteEnvio = 45LL * 1024 * 1024;  // API do Telegram: 50 MB

QString apiUrl(const QString &token, const QString &metodo)
{
    return QStringLiteral("https://api.telegram.org/bot%1/%2").arg(token, metodo);
}
} // namespace

TelegramService::TelegramService(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString TelegramService::token() const
{
    return QSettings().value(QLatin1String(kChaveToken)).toString();
}

QString TelegramService::chatId() const
{
    return QSettings().value(QLatin1String(kChaveChat)).toString();
}

bool TelegramService::ativo() const
{
    return QSettings().value(QLatin1String(kChaveAtivo), true).toBool();
}

bool TelegramService::enviaBackup() const
{
    return QSettings().value(QLatin1String(kChaveBackup), true).toBool();
}

bool TelegramService::configurado() const
{
    return !token().trimmed().isEmpty() && !chatId().trimmed().isEmpty();
}

void TelegramService::salvarConfig(const QString &token, const QString &chatId,
                                   bool ativo, bool enviaBackup)
{
    QSettings s;
    s.setValue(QLatin1String(kChaveToken), token.trimmed());
    s.setValue(QLatin1String(kChaveChat), chatId.trimmed());
    s.setValue(QLatin1String(kChaveAtivo), ativo);
    s.setValue(QLatin1String(kChaveBackup), enviaBackup);
    s.sync();
}

void TelegramService::enviarMensagem(const QString &texto)
{
    if (!configurado()) {
        emit resultado(false, QStringLiteral("Telegram não configurado."));
        return;
    }

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("chat_id"), chatId());
    form.addQueryItem(QStringLiteral("text"), texto);
    form.addQueryItem(QStringLiteral("parse_mode"), QStringLiteral("HTML"));
    form.addQueryItem(QStringLiteral("disable_web_page_preview"), QStringLiteral("true"));

    QNetworkRequest req{QUrl(apiUrl(token(), QStringLiteral("sendMessage")))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QNetworkReply *reply = m_net->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // A API devolve o motivo em JSON (ex.: chat não encontrado).
            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            const QString desc = o.value(QStringLiteral("description")).toString();
            emit resultado(false, desc.isEmpty() ? reply->errorString() : desc);
            return;
        }
        emit resultado(true, QStringLiteral("Mensagem enviada."));
    });
}

void TelegramService::enviarArquivo(const QString &caminho, const QString &legenda)
{
    if (!configurado() || !QFileInfo::exists(caminho))
        return;
    if (QFileInfo(caminho).size() > kLimiteEnvio) {
        emit resultado(false, QStringLiteral(
            "Arquivo grande demais para o Telegram (%1 MB). Backup não enviado — "
            "copie manualmente para um pen drive.")
            .arg(QFileInfo(caminho).size() / (1024 * 1024)));
        return;
    }

    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    const auto campoTexto = [multi](const QString &nome, const QString &valor) {
        QHttpPart p;
        p.setHeader(QNetworkRequest::ContentDispositionHeader,
                    QStringLiteral("form-data; name=\"%1\"").arg(nome));
        p.setBody(valor.toUtf8());
        multi->append(p);
    };
    campoTexto(QStringLiteral("chat_id"), chatId());
    campoTexto(QStringLiteral("caption"), legenda);

    auto *f = new QFile(caminho);
    if (!f->open(QIODevice::ReadOnly)) {
        delete f;
        delete multi;
        return;
    }
    QHttpPart arquivo;
    arquivo.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/html"));
    arquivo.setHeader(QNetworkRequest::ContentDispositionHeader,
                      QStringLiteral("form-data; name=\"document\"; filename=\"%1\"")
                          .arg(QFileInfo(caminho).fileName()));
    arquivo.setBodyDevice(f);
    f->setParent(multi);   // o multipart passa a ser dono do arquivo
    multi->append(arquivo);

    QNetworkRequest req{QUrl(apiUrl(token(), QStringLiteral("sendDocument")))};
    QNetworkReply *reply = m_net->post(req, multi);
    multi->setParent(reply);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void TelegramService::descobrirChat(const QString &tokenInformado)
{
    const QString tk = tokenInformado.trimmed().isEmpty() ? token() : tokenInformado.trimmed();
    if (tk.isEmpty()) {
        emit resultado(false, QStringLiteral("Informe o token do bot primeiro."));
        return;
    }

    // allowed_updates explícito: além das mensagens, queremos o evento de
    // "bot foi adicionado ao grupo" (my_chat_member) — ele chega mesmo com o
    // privacy mode ligado, que por padrão esconde as mensagens comuns do grupo.
    QUrl url(apiUrl(tk, QStringLiteral("getUpdates")));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("allowed_updates"),
                        QStringLiteral(R"(["message","channel_post","my_chat_member"])"));
    params.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    url.setQuery(params);

    QNetworkRequest req{url};
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QJsonObject raiz = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || !raiz.value(QStringLiteral("ok")).toBool()) {
            const QString desc = raiz.value(QStringLiteral("description")).toString();
            emit resultado(false, desc.isEmpty()
                                      ? QStringLiteral("Não consegui falar com o Telegram. Confira o token.")
                                      : desc);
            return;
        }

        // Percorre de trás para frente: queremos a conversa mais recente.
        const QJsonArray updates = raiz.value(QStringLiteral("result")).toArray();
        for (int i = updates.size() - 1; i >= 0; --i) {
            const QJsonObject up = updates.at(i).toObject();
            for (const QString &campo : {QStringLiteral("message"),
                                         QStringLiteral("channel_post"),
                                         QStringLiteral("my_chat_member")}) {
                const QJsonObject chat = up.value(campo).toObject()
                                             .value(QStringLiteral("chat")).toObject();
                if (chat.isEmpty())
                    continue;
                const QString id = QString::number(
                    static_cast<qint64>(chat.value(QStringLiteral("id")).toDouble()));
                QString nome = chat.value(QStringLiteral("title")).toString();
                if (nome.isEmpty())
                    nome = chat.value(QStringLiteral("first_name")).toString();
                if (nome.isEmpty())
                    nome = chat.value(QStringLiteral("username")).toString();
                emit chatDescoberto(id, nome);
                return;
            }
        }
        emit resultado(false, QStringLiteral(
            "Nenhuma conversa encontrada. No GRUPO, mande uma mensagem começando "
            "com barra — por exemplo /oi — e clique de novo. (Por padrão o bot não "
            "enxerga mensagens comuns do grupo, só as que começam com /.)"));
    });
}
