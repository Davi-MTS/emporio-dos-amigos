#include "services/log/LogService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

namespace {

constexpr qint64 kTamanhoMaximo = 2 * 1024 * 1024;  // 2 MB por arquivo
constexpr int kArquivosAntigos = 3;                 // sistema.1.log ... .3.log

QMutex g_mutex;                 // o Qt pode logar de várias threads
QtMessageHandler g_anterior = nullptr;

QString nivelTexto(QtMsgType tipo)
{
    switch (tipo) {
    case QtDebugMsg:    return QStringLiteral("info");
    case QtInfoMsg:     return QStringLiteral("info");
    case QtWarningMsg:  return QStringLiteral("AVISO");
    case QtCriticalMsg: return QStringLiteral("ERRO");
    case QtFatalMsg:    return QStringLiteral("FATAL");
    }
    return QStringLiteral("info");
}

// Mantém o log de crescer para sempre: ao passar do limite, vira sistema.1.log
// e os anteriores descem uma posição; o mais antigo é descartado.
void rotacionarSePreciso()
{
    const QString base = LogService::caminhoArquivo();
    if (QFileInfo(base).size() < kTamanhoMaximo)
        return;

    const QString semExt = base.left(base.size() - 4);   // tira ".log"
    QFile::remove(QStringLiteral("%1.%2.log").arg(semExt).arg(kArquivosAntigos));
    for (int i = kArquivosAntigos - 1; i >= 1; --i) {
        const QString de = QStringLiteral("%1.%2.log").arg(semExt).arg(i);
        const QString para = QStringLiteral("%1.%2.log").arg(semExt).arg(i + 1);
        if (QFile::exists(de))
            QFile::rename(de, para);
    }
    QFile::rename(base, QStringLiteral("%1.1.log").arg(semExt));
}

void escrever(const QString &nivel, const QString &texto, const QString &origem = QString())
{
    QMutexLocker trava(&g_mutex);

    QDir().mkpath(LogService::pasta());
    rotacionarSePreciso();

    QFile f(LogService::caminhoArquivo());
    const bool novo = !f.exists() || f.size() == 0;
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    // Marca de UTF-8 no começo do arquivo: sem ela, ferramentas antigas do
    // Windows (e o Bloco de Notas de versões velhas) mostram os acentos
    // trocados — e é o dono da loja que vai abrir este arquivo.
    if (novo)
        out.setGenerateByteOrderMark(true);
    out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        << QStringLiteral("  [") << nivel << QStringLiteral("]  ") << texto;
    if (!origem.isEmpty())
        out << QStringLiteral("   (") << origem << QLatin1Char(')');
    out << Qt::endl;
    f.close();
}

void captador(QtMsgType tipo, const QMessageLogContext &ctx, const QString &msg)
{
    // Ruído conhecido do ambiente, não é problema do sistema.
    if (msg.contains(QLatin1String("QFontDatabase"))
        || msg.contains(QLatin1String("no longer ships fonts"))) {
        if (g_anterior)
            g_anterior(tipo, ctx, msg);
        return;
    }

    QString origem;
    if (ctx.file && *ctx.file)
        origem = QStringLiteral("%1:%2").arg(QString::fromUtf8(ctx.file)).arg(ctx.line);

    escrever(nivelTexto(tipo), msg, origem);

    // Mantém o comportamento padrão (console no build de desenvolvimento).
    if (g_anterior)
        g_anterior(tipo, ctx, msg);
}

} // namespace

namespace LogService {

QString pasta()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.distribuidora");
    return base + QStringLiteral("/logs");
}

QString caminhoArquivo()
{
    return pasta() + QStringLiteral("/sistema.log");
}

void instalar()
{
    g_anterior = qInstallMessageHandler(captador);
    escrever(QStringLiteral("info"),
             QStringLiteral("=== Sistema iniciado (versão %1) ===")
                 .arg(QCoreApplication::applicationVersion()));
}

void registrar(const QString &mensagem)
{
    escrever(QStringLiteral("info"), mensagem);
}

QStringList ultimasLinhas(int n)
{
    QFile f(caminhoArquivo());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    QStringList todas;
    while (!in.atEnd()) {
        const QString l = in.readLine();
        if (!l.trimmed().isEmpty())
            todas << l;
    }
    f.close();

    QStringList ultimas;
    for (int i = todas.size() - 1; i >= 0 && ultimas.size() < n; --i)
        ultimas << todas.at(i);
    return ultimas;
}

} // namespace LogService
