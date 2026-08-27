#include "services/backup/BackupService.h"

#include <algorithm>
#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

BackupService::BackupService(QSqlDatabase db, const QString &backupsDir)
    : m_db(std::move(db))
    , m_dir(backupsDir.isEmpty() ? pastaPadrao() : backupsDir)
{
}

QString BackupService::pastaPadrao()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/Empório dos Amigos/Backups");
}

QString BackupService::marcadorPath(const QString &dbPath)
{
    return dbPath + QStringLiteral(".restore");
}

bool BackupService::criarBackup(BackupInfo *out)
{
    if (!QDir().mkpath(m_dir)) {
        m_erro = QStringLiteral("Não foi possível criar a pasta de backups: %1").arg(m_dir);
        return false;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString dbDest = m_dir + QStringLiteral("/distribuidora-") + stamp + QStringLiteral(".db");
    for (int i = 1; QFileInfo::exists(dbDest); ++i) // evita colisão no mesmo segundo
        dbDest = m_dir + QStringLiteral("/distribuidora-") + stamp
                 + QStringLiteral("-%1.db").arg(i);

    // VACUUM INTO: snapshot único e consistente (lê inclusive o WAL), sem fechar
    // a conexão. Aspas simples do caminho são duplicadas por segurança.
    const QString destEsc = QString(dbDest).replace(QLatin1Char('\''), QStringLiteral("''"));
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("VACUUM INTO '%1'").arg(destEsc))) {
        m_erro = q.lastError().text();
        return false;
    }

    BackupInfo info;
    info.caminho = dbDest;
    info.criadoEm = QDateTime::currentDateTime().toString(Qt::ISODate);
    info.tamanho = QFileInfo(dbDest).size();
    info.contagens = coletarContagens();
    info.resumo = resumoDe(info.contagens);
    escreverSidecar(dbDest, info);

    if (out)
        *out = info;
    m_erro.clear();
    return true;
}

QVariantMap BackupService::coletarContagens() const
{
    QVariantMap m;
    const auto conta = [this](const char *tabela) -> qint64 {
        QSqlQuery q(m_db);
        if (q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(QLatin1String(tabela)))
            && q.next())
            return q.value(0).toLongLong();
        return 0;
    };
    m[QStringLiteral("vendas")] = conta("vendas");
    m[QStringLiteral("produtos")] = conta("produtos");
    m[QStringLiteral("clientes")] = conta("clientes");
    m[QStringLiteral("compras")] = conta("compras");
    return m;
}

QString BackupService::resumoDe(const QVariantMap &c)
{
    return QStringLiteral("%1 vendas · %2 produtos · %3 clientes")
        .arg(c.value(QStringLiteral("vendas")).toLongLong())
        .arg(c.value(QStringLiteral("produtos")).toLongLong())
        .arg(c.value(QStringLiteral("clientes")).toLongLong());
}

void BackupService::escreverSidecar(const QString &dbPath, const BackupInfo &info) const
{
    QJsonObject o;
    o[QStringLiteral("criadoEm")] = info.criadoEm;
    o[QStringLiteral("tamanho")] = static_cast<double>(info.tamanho);
    o[QStringLiteral("resumo")] = info.resumo;
    o[QStringLiteral("versaoApp")] = QCoreApplication::applicationVersion();
    QJsonObject cont;
    for (auto it = info.contagens.constBegin(); it != info.contagens.constEnd(); ++it)
        cont[it.key()] = static_cast<double>(it.value().toLongLong());
    o[QStringLiteral("contagens")] = cont;

    QFile f(dbPath + QStringLiteral(".json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        f.close();
    }
}

BackupInfo BackupService::lerSidecar(const QString &dbPath)
{
    BackupInfo info;
    QFile f(dbPath + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly))
        return info;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return info;
    const QJsonObject o = doc.object();
    info.criadoEm = o.value(QStringLiteral("criadoEm")).toString();
    info.tamanho = static_cast<qint64>(o.value(QStringLiteral("tamanho")).toDouble());
    info.resumo = o.value(QStringLiteral("resumo")).toString();
    const QJsonObject cont = o.value(QStringLiteral("contagens")).toObject();
    for (auto it = cont.constBegin(); it != cont.constEnd(); ++it)
        info.contagens[it.key()] = static_cast<qint64>(it.value().toDouble());
    return info;
}

QVector<BackupInfo> BackupService::listarBackups() const
{
    QVector<BackupInfo> lista;
    QDir d(m_dir);
    if (!d.exists())
        return lista;

    const auto arquivos = d.entryList(QStringList{QStringLiteral("*.db")}, QDir::Files);
    for (const QString &nome : arquivos) {
        const QString full = d.absoluteFilePath(nome);
        BackupInfo info = lerSidecar(full);
        info.caminho = full;
        const QFileInfo fi(full);
        if (info.tamanho == 0)
            info.tamanho = fi.size();
        if (info.criadoEm.isEmpty())
            info.criadoEm = fi.lastModified().toString(Qt::ISODate);
        if (info.resumo.isEmpty())
            info.resumo = QStringLiteral("(sem detalhes)");
        lista.push_back(info);
    }

    std::sort(lista.begin(), lista.end(),
              [](const BackupInfo &a, const BackupInfo &b) { return a.criadoEm > b.criadoEm; });
    return lista;
}

bool BackupService::rotacionar(int manter)
{
    if (manter < 0)
        return true;
    const QVector<BackupInfo> lista = listarBackups(); // mais recente primeiro
    for (int i = manter; i < lista.size(); ++i) {
        QFile::remove(lista.at(i).caminho);
        QFile::remove(lista.at(i).caminho + QStringLiteral(".json"));
    }
    return true;
}

bool BackupService::agendarRestauracao(const QString &caminhoBackup)
{
    if (!QFileInfo::exists(caminhoBackup)) {
        m_erro = QStringLiteral("Backup não encontrado.");
        return false;
    }
    // Backup de emergência do estado atual (para poder desfazer a restauração).
    if (!criarBackup(nullptr))
        return false; // m_erro já preenchido

    const QString dbPath = m_db.databaseName();
    QFile marcador(marcadorPath(dbPath));
    if (!marcador.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_erro = QStringLiteral("Não foi possível agendar a restauração.");
        return false;
    }
    marcador.write(caminhoBackup.toUtf8());
    marcador.close();
    m_erro.clear();
    return true;
}

bool BackupService::aplicarRestauracaoPendente(const QString &dbPath, QString *erro)
{
    const QString marc = marcadorPath(dbPath);
    if (!QFileInfo::exists(marc))
        return true; // nada agendado

    QString backupPath;
    {
        QFile mf(marc);
        if (!mf.open(QIODevice::ReadOnly)) {
            if (erro)
                *erro = QStringLiteral("Marcador de restauração ilegível.");
            return false;
        }
        backupPath = QString::fromUtf8(mf.readAll()).trimmed();
        mf.close();
    }

    if (backupPath.isEmpty() || !QFileInfo::exists(backupPath)) {
        QFile::remove(marc);
        if (erro)
            *erro = QStringLiteral("Backup agendado não existe mais.");
        return false;
    }

    // Substitui o banco vivo pela cópia (removendo os sidecars do WAL).
    QFile::remove(dbPath);
    QFile::remove(dbPath + QStringLiteral("-wal"));
    QFile::remove(dbPath + QStringLiteral("-shm"));
    const bool ok = QFile::copy(backupPath, dbPath);
    QFile::remove(marc);
    if (!ok && erro)
        *erro = QStringLiteral("Falha ao restaurar (copiar o arquivo do backup).");
    return ok;
}
